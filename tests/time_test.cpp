/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <time.h>

#include <errno.h>
#include <features.h>
#include <gtest/gtest.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ScopedSignalHandler.h"

#if defined(__BIONIC__) // mktime_tz is a bionic extension.
#include <libc/private/bionic_time.h>
#endif // __BIONIC__

TEST(time, mktime_tz) {
#if defined(__BIONIC__)
  struct tm epoch;
  memset(&epoch, 0, sizeof(tm));
  epoch.tm_year = 1970 - 1900;
  epoch.tm_mon = 1;
  epoch.tm_mday = 1;

  // Alphabetically first. Coincidentally equivalent to UTC.
  ASSERT_EQ(2678400, mktime_tz(&epoch, "Africa/Abidjan"));

  // Alphabetically last. Coincidentally equivalent to UTC.
  ASSERT_EQ(2678400, mktime_tz(&epoch, "Zulu"));

  // Somewhere in the middle, not UTC.
  ASSERT_EQ(2707200, mktime_tz(&epoch, "America/Los_Angeles"));

  // Missing. Falls back to UTC.
  ASSERT_EQ(2678400, mktime_tz(&epoch, "PST"));
#else // __BIONIC__
  GTEST_LOG_(INFO) << "This test does nothing.\n";
#endif // __BIONIC__
}

TEST(time, gmtime) {
  time_t t = 0;
  tm* broken_down = gmtime(&t);
  ASSERT_TRUE(broken_down != NULL);
  ASSERT_EQ(0, broken_down->tm_sec);
  ASSERT_EQ(0, broken_down->tm_min);
  ASSERT_EQ(0, broken_down->tm_hour);
  ASSERT_EQ(1, broken_down->tm_mday);
  ASSERT_EQ(0, broken_down->tm_mon);
  ASSERT_EQ(1970, broken_down->tm_year + 1900);
}

static void* gmtime_no_stack_overflow_14313703_fn(void*) {
  const char* original_tz = getenv("TZ");
  // Ensure we'll actually have to enter tzload by using a time zone that doesn't exist.
  setenv("TZ", "gmtime_stack_overflow_14313703", 1);
  tzset();
  if (original_tz != NULL) {
    setenv("TZ", original_tz, 1);
  }
  tzset();
  return NULL;
}

TEST(time, gmtime_no_stack_overflow_14313703) {
  // Is it safe to call tzload on a thread with a small stack?
  // http://b/14313703
  // https://code.google.com/p/android/issues/detail?id=61130
  pthread_attr_t attributes;
  ASSERT_EQ(0, pthread_attr_init(&attributes));
#if defined(__BIONIC__)
  ASSERT_EQ(0, pthread_attr_setstacksize(&attributes, PTHREAD_STACK_MIN));
#else
  // PTHREAD_STACK_MIN not currently in the host GCC sysroot.
  ASSERT_EQ(0, pthread_attr_setstacksize(&attributes, 4 * getpagesize()));
#endif

  pthread_t t;
  ASSERT_EQ(0, pthread_create(&t, &attributes, gmtime_no_stack_overflow_14313703_fn, NULL));
  void* result;
  ASSERT_EQ(0, pthread_join(t, &result));
}

TEST(time, mktime_10310929) {
  struct tm t;
  memset(&t, 0, sizeof(tm));
  t.tm_year = 200;
  t.tm_mon = 2;
  t.tm_mday = 10;

#if !defined(__LP64__)
  // 32-bit bionic stupidly had a signed 32-bit time_t.
  ASSERT_EQ(-1, mktime(&t));
#if defined(__BIONIC__)
  ASSERT_EQ(-1, mktime_tz(&t, "UTC"));
#endif
#else
  // Everyone else should be using a signed 64-bit time_t.
  ASSERT_GE(sizeof(time_t) * 8, 64U);

  setenv("TZ", "America/Los_Angeles", 1);
  tzset();
  ASSERT_EQ(static_cast<time_t>(4108348800U), mktime(&t));
#if defined(__BIONIC__)
  ASSERT_EQ(static_cast<time_t>(4108320000U), mktime_tz(&t, "UTC"));
#endif

  setenv("TZ", "UTC", 1);
  tzset();
  ASSERT_EQ(static_cast<time_t>(4108320000U), mktime(&t));
#if defined(__BIONIC__)
  ASSERT_EQ(static_cast<time_t>(4108348800U), mktime_tz(&t, "America/Los_Angeles"));
#endif
#endif
}

TEST(time, strftime) {
  setenv("TZ", "UTC", 1);

  struct tm t;
  memset(&t, 0, sizeof(tm));
  t.tm_year = 200;
  t.tm_mon = 2;
  t.tm_mday = 10;

  char buf[64];

  // Seconds since the epoch.
#if defined(__BIONIC__) || defined(__LP64__) // Not 32-bit glibc.
  EXPECT_EQ(10U, strftime(buf, sizeof(buf), "%s", &t));
  EXPECT_STREQ("4108320000", buf);
#endif

  // Date and time as text.
  EXPECT_EQ(24U, strftime(buf, sizeof(buf), "%c", &t));
  EXPECT_STREQ("Sun Mar 10 00:00:00 2100", buf);
}

TEST(time, strptime) {
  setenv("TZ", "UTC", 1);

  struct tm t;
  char buf[64];

  memset(&t, 0, sizeof(t));
  strptime("11:14", "%R", &t);
  strftime(buf, sizeof(buf), "%H:%M", &t);
  EXPECT_STREQ("11:14", buf);

  memset(&t, 0, sizeof(t));
  strptime("09:41:53", "%T", &t);
  strftime(buf, sizeof(buf), "%H:%M:%S", &t);
  EXPECT_STREQ("09:41:53", buf);
}

void SetTime(timer_t t, time_t value_s, time_t value_ns, time_t interval_s, time_t interval_ns) {
  itimerspec ts;
  ts.it_value.tv_sec = value_s;
  ts.it_value.tv_nsec = value_ns;
  ts.it_interval.tv_sec = interval_s;
  ts.it_interval.tv_nsec = interval_ns;
  ASSERT_EQ(0, timer_settime(t, TIMER_ABSTIME, &ts, NULL));
}

static void NoOpNotifyFunction(sigval_t) {
}

TEST(time, timer_create) {
  sigevent_t se;
  memset(&se, 0, sizeof(se));
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_notify_function = NoOpNotifyFunction;
  timer_t timer_id;
  ASSERT_EQ(0, timer_create(CLOCK_MONOTONIC, &se, &timer_id));

  int pid = fork();
  ASSERT_NE(-1, pid) << strerror(errno);

  if (pid == 0) {
    // Timers are not inherited by the child.
    ASSERT_EQ(-1, timer_delete(timer_id));
    ASSERT_EQ(EINVAL, errno);
    _exit(0);
  }

  int status;
  ASSERT_EQ(pid, waitpid(pid, &status, 0));
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(0, WEXITSTATUS(status));

  ASSERT_EQ(0, timer_delete(timer_id));
}

static int timer_create_SIGEV_SIGNAL_signal_handler_invocation_count = 0;
static void timer_create_SIGEV_SIGNAL_signal_handler(int signal_number) {
  ++timer_create_SIGEV_SIGNAL_signal_handler_invocation_count;
  ASSERT_EQ(SIGUSR1, signal_number);
}

TEST(time, timer_create_SIGEV_SIGNAL) {
  sigevent_t se;
  memset(&se, 0, sizeof(se));
  se.sigev_notify = SIGEV_SIGNAL;
  se.sigev_signo = SIGUSR1;

  timer_t timer_id;
  ASSERT_EQ(0, timer_create(CLOCK_MONOTONIC, &se, &timer_id));

  ScopedSignalHandler ssh(SIGUSR1, timer_create_SIGEV_SIGNAL_signal_handler);

  ASSERT_EQ(0, timer_create_SIGEV_SIGNAL_signal_handler_invocation_count);

  itimerspec ts;
  ts.it_value.tv_sec =  0;
  ts.it_value.tv_nsec = 1;
  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;
  ASSERT_EQ(0, timer_settime(timer_id, TIMER_ABSTIME, &ts, NULL));

  usleep(500000);
  ASSERT_EQ(1, timer_create_SIGEV_SIGNAL_signal_handler_invocation_count);
}

struct Counter {
  volatile int value;
  timer_t timer_id;
  sigevent_t se;

  Counter(void (*fn)(sigval_t)) : value(0) {
    memset(&se, 0, sizeof(se));
    se.sigev_notify = SIGEV_THREAD;
    se.sigev_notify_function = fn;
    se.sigev_value.sival_ptr = this;
  }

  void Create() {
    ASSERT_EQ(0, timer_create(CLOCK_REALTIME, &se, &timer_id));
  }

  ~Counter() {
    if (timer_delete(timer_id) != 0) {
      abort();
    }
  }

  static void CountNotifyFunction(sigval_t value) {
    Counter* cd = reinterpret_cast<Counter*>(value.sival_ptr);
    ++cd->value;
  }

  static void CountAndDisarmNotifyFunction(sigval_t value) {
    Counter* cd = reinterpret_cast<Counter*>(value.sival_ptr);
    ++cd->value;

    // Setting the initial expiration time to 0 disarms the timer.
    SetTime(cd->timer_id, 0, 0, 1, 0);
  }
};

TEST(time, timer_settime_0) {
  Counter counter(Counter::CountAndDisarmNotifyFunction);
  counter.Create();

  ASSERT_EQ(0, counter.value);

  SetTime(counter.timer_id, 0, 1, 1, 0);
  usleep(500000);

  // The count should just be 1 because we disarmed the timer the first time it fired.
  ASSERT_EQ(1, counter.value);
}

TEST(time, timer_settime_repeats) {
  Counter counter(Counter::CountNotifyFunction);
  counter.Create();

  ASSERT_EQ(0, counter.value);

  SetTime(counter.timer_id, 0, 1, 0, 10);
  usleep(500000);

  // The count should just be > 1 because we let the timer repeat.
  ASSERT_GT(counter.value, 1);
}

static int timer_create_NULL_signal_handler_invocation_count = 0;
static void timer_create_NULL_signal_handler(int signal_number) {
  ++timer_create_NULL_signal_handler_invocation_count;
  ASSERT_EQ(SIGALRM, signal_number);
}

TEST(time, timer_create_NULL) {
  // A NULL sigevent* is equivalent to asking for SIGEV_SIGNAL for SIGALRM.
  timer_t timer_id;
  ASSERT_EQ(0, timer_create(CLOCK_MONOTONIC, NULL, &timer_id));

  ScopedSignalHandler ssh(SIGALRM, timer_create_NULL_signal_handler);

  ASSERT_EQ(0, timer_create_NULL_signal_handler_invocation_count);

  SetTime(timer_id, 0, 1, 0, 0);
  usleep(500000);

  ASSERT_EQ(1, timer_create_NULL_signal_handler_invocation_count);
}

TEST(time, timer_create_EINVAL) {
  clockid_t invalid_clock = 16;

  // A SIGEV_SIGNAL timer is easy; the kernel does all that.
  timer_t timer_id;
  ASSERT_EQ(-1, timer_create(invalid_clock, NULL, &timer_id));
  ASSERT_EQ(EINVAL, errno);

  // A SIGEV_THREAD timer is more interesting because we have stuff to clean up.
  sigevent_t se;
  memset(&se, 0, sizeof(se));
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_notify_function = NoOpNotifyFunction;
  ASSERT_EQ(-1, timer_create(invalid_clock, &se, &timer_id));
  ASSERT_EQ(EINVAL, errno);
}

TEST(time, timer_delete_multiple) {
  timer_t timer_id;
  ASSERT_EQ(0, timer_create(CLOCK_MONOTONIC, NULL, &timer_id));
  ASSERT_EQ(0, timer_delete(timer_id));
  ASSERT_EQ(-1, timer_delete(timer_id));
  ASSERT_EQ(EINVAL, errno);

  sigevent_t se;
  memset(&se, 0, sizeof(se));
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_notify_function = NoOpNotifyFunction;
  ASSERT_EQ(0, timer_create(CLOCK_MONOTONIC, &se, &timer_id));
  ASSERT_EQ(0, timer_delete(timer_id));
  ASSERT_EQ(-1, timer_delete(timer_id));
  ASSERT_EQ(EINVAL, errno);
}

TEST(time, timer_create_multiple) {
  Counter counter1(Counter::CountNotifyFunction);
  counter1.Create();
  Counter counter2(Counter::CountNotifyFunction);
  counter2.Create();
  Counter counter3(Counter::CountNotifyFunction);
  counter3.Create();

  ASSERT_EQ(0, counter1.value);
  ASSERT_EQ(0, counter2.value);
  ASSERT_EQ(0, counter3.value);

  SetTime(counter2.timer_id, 0, 1, 0, 0);
  usleep(500000);

  EXPECT_EQ(0, counter1.value);
  EXPECT_EQ(1, counter2.value);
  EXPECT_EQ(0, counter3.value);
}

struct TimerDeleteData {
  timer_t timer_id;
  pthread_t thread_id;
  volatile bool complete;
};

static void TimerDeleteCallback(sigval_t value) {
  TimerDeleteData* tdd = reinterpret_cast<TimerDeleteData*>(value.sival_ptr);

  tdd->thread_id = pthread_self();
  timer_delete(tdd->timer_id);
  tdd->complete = true;
}

TEST(time, timer_delete_from_timer_thread) {
  TimerDeleteData tdd;
  sigevent_t se;

  memset(&se, 0, sizeof(se));
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_notify_function = TimerDeleteCallback;
  se.sigev_value.sival_ptr = &tdd;

  tdd.complete = false;
  ASSERT_EQ(0, timer_create(CLOCK_REALTIME, &se, &tdd.timer_id));

  itimerspec ts;
  ts.it_value.tv_sec = 0;
  ts.it_value.tv_nsec = 100;
  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;
  ASSERT_EQ(0, timer_settime(tdd.timer_id, TIMER_ABSTIME, &ts, NULL));

  time_t cur_time = time(NULL);
  while (!tdd.complete && (time(NULL) - cur_time) < 5);
  ASSERT_TRUE(tdd.complete);

#if defined(__BIONIC__)
  // Since bionic timers are implemented by creating a thread to handle the
  // callback, verify that the thread actually completes.
  cur_time = time(NULL);
  while (pthread_detach(tdd.thread_id) != ESRCH && (time(NULL) - cur_time) < 5);
  ASSERT_EQ(ESRCH, pthread_detach(tdd.thread_id));
#endif
}
