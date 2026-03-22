#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems.h>
#include <rtems/shell.h>
#include <stdio.h>

static rtems_task Init(rtems_task_argument ignored)
{
  (void) ignored;

  printf("Starting RTEMS Shell...\n");

  rtems_shell_init(
    "SHLL",
    16 * 1024,
    100,
    "/dev/console",
    false,
    true,
    NULL
  );

  while (1) {
    rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(1000));
  }
}

/* Drivers */
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_SIMPLE_CONSOLE_DRIVER

/* Shell */
#define CONFIGURE_SHELL_COMMANDS_INIT
#define CONFIGURE_SHELL_COMMANDS_ALL

/* Block layer */
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK
#define CONFIGURE_BDBUF_BUFFER_MIN_SIZE 512
#define CONFIGURE_BDBUF_BUFFER_MAX_SIZE 512
#define CONFIGURE_BDBUF_CACHE_MEMORY_SIZE (128 * 1024)

/* Resources */
#define CONFIGURE_MAXIMUM_TASKS 20
#define CONFIGURE_MAXIMUM_SEMAPHORES 20
#define CONFIGURE_MAXIMUM_POSIX_THREADS 4
#define CONFIGURE_MAXIMUM_POSIX_KEYS 4
#define CONFIGURE_MAXIMUM_POSIX_KEY_VALUE_PAIRS 8
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 32

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT

#include <rtems/confdefs.h>