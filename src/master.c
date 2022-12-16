#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// Variables to store the PIDs
pid_t pid_cmd;
pid_t pid_mx;
pid_t pid_mz;
pid_t pid_world;
pid_t pid_insp;

// Variable to store the status of the child process
int status;

// Function to fork and create a child process
int spawn(const char *program, char *arg_list[])
{

  pid_t child_pid = fork();

  // If fork() returns a negative value, the fork failed.
  if (child_pid < 0)
  {
    return -1;
  }

  // If fork() returns a positive value, we are in the parent process.
  else if (child_pid != 0)
  {
    return child_pid;
  }

  // If fork() returns 0, we are in the child process.
  else
  {
    if (execvp(program, arg_list) == 0)
      ;

    // If execvp() returns, it must have failed.
    return -1;
  }
}

// Function to create all the log files
int create_log_files()
{
  // Create the log files
  int fd_cmd = open("./log/command.log", O_CREAT | O_RDWR, 0666);
  int fd_mx = open("./log/mx.log", O_CREAT | O_RDWR, 0666);
  int fd_mz = open("./log/mz.log", O_CREAT | O_RDWR, 0666);
  int fd_world = open("./log/world.log", O_CREAT | O_RDWR, 0666);
  int fd_insp = open("./log/inspection.log", O_CREAT | O_RDWR, 0666);

  // Check if the log files were created successfully
  if (fd_cmd < 0 || fd_mx < 0 || fd_mz < 0 || fd_world < 0 || fd_insp < 0)
  {
    return 1;
  }
  // Close the log files
  close(fd_cmd);
  close(fd_mx);
  close(fd_mz);
  close(fd_world);
  close(fd_insp);

  return 0;
}

// Function to get when a file was last modified
time_t get_last_modified(char *filename)
{
  struct stat attr;
  if (stat(filename, &attr) == -1)
  {
    return -1;
  }
  return attr.st_mtime;
}

// Function to kill all the child processes
void kill_all()
{
  kill(pid_cmd, SIGKILL);
  kill(pid_mx, SIGKILL);
  kill(pid_mz, SIGKILL);
  kill(pid_world, SIGKILL);
  kill(pid_insp, SIGKILL);
}

// Function to control the child processes
// It will check if the child processes are writing to the log files
// If they are not, it will kill them
// It will also check if the child processes are still alive
// If at least one of the processes terminated unexpectedly, it will kill the others
int watchdog()
{
  // Array of the log file paths
  char *log_files[5] = {"./log/command.log", "./log/mx.log", "./log/mz.log", "./log/world.log", "./log/inspection.log"};

  // Array of the PIDs
  pid_t pids[5] = {pid_cmd, pid_mx, pid_mz, pid_world, pid_insp};

  // Flag to check if a file was modified
  int mod_flag = 0;

  // Variable to keep the number of seconds since the last modification
  int seconds_since_mod = 0;

  // Infinite loop
  while (1)
  {
    // Get current time
    time_t current_time = time(NULL);

    // Loop through the log files
    for (int i = 0; i < 5; i++)
    {
      // Get the last modified time of the log file
      time_t last_modified = get_last_modified(log_files[i]);

      // If the last modified time is -1, an error occurred
      if (last_modified == -1)
      {
        // Kill all the child processes
        kill_all();
        // Return error
        return 1;
      }

      // Check if the file was modified in the last 2 seconds
      if (current_time - last_modified > 2)
      {
        // If the file wasn't modified in the last 2 seconds, set the flag to 0
        mod_flag = 0;
      }
      else
      {
        // If the file was modified in the last 2 seconds, set the flag to 1 and reset the counter
        mod_flag = 1;
        seconds_since_mod = 0;
      }
    }

    // If the flag is 0, increment the counter
    if (mod_flag == 0)
    {
      seconds_since_mod += 2;
    }

    // Variable to store the return value of the waitpid() function
    int ret;

    // Check if a child process terminated unexpectedly
    if ((ret = waitpid(-1, &status, WNOHANG)) > 0)
    {
      // If a child process terminated unexpectedly, kill all the child processes
      kill_all();
      // Return error -1
      return -1;
    }
    else if (ret == -1)
    {
      // If an error occurred, kill all the child processes
      kill_all();
      // If waitpid() returns -1, return error
      return 1;
    }

    // If the counter is greater than 60, kill the child processes
    if (seconds_since_mod > 60)
    {
      kill_all();
      return 0;
    }

    // Sleep for 2 seconds
    sleep(2);
  }
}

int main()
{

  // File descriptor for the log file
  int log_fd;

  // Open the log file
  if ((log_fd = open("log/master.log", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1)
  {
    // If the file could not be opened, print an error message and exit
    perror("Error opening log file");
    return 1;
  }

  // Log that master process has started and date and time
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char buffer[100];
  sprintf(buffer, "%d-%d-%d %d:%d:%d: <master_process> Master process started\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  int m = write(log_fd, buffer, strlen(buffer));

  // Check for errors
  if (m == -1 || m != strlen(buffer))
  {
    // If an error occurred, print an error message and exit
    perror("Error writing to log file");
    // Close the log file
    close(log_fd);
    return 1;
  }

  // Command console process
  char *arg_list_command[] = {"/usr/bin/konsole", "-e", "./bin/command", NULL};
  pid_cmd = spawn("/usr/bin/konsole", arg_list_command);
  if (pid_cmd == -1)
  {
    // Go to spawn_err if spawn() returns -1
    goto spawn_err;
  }

  // Mx process
  char *arg_list_mx[] = {"./bin/mx", NULL};
  pid_mx = spawn("./bin/mx", arg_list_mx);
  if (pid_mx == -1)
  {
    // Go to spawn_err if spawn() returns -1
    goto spawn_err;
  }
  // Convert pid to string to pass as argument to inspection process
  char pid_mx_str[10];
  sprintf(pid_mx_str, "%d", pid_mx);

  // Mz process
  char *arg_list_mz[] = {"./bin/mz", NULL};
  pid_mz = spawn("./bin/mz", arg_list_mz);
  if (pid_mz == -1)
  {
    // Go to spawn_err if spawn() returns -1
    goto spawn_err;
  }
  // Convert pid to string to pass as argument to inspection process
  char pid_mz_str[10];
  sprintf(pid_mz_str, "%d", pid_mz);

  // World process
  char *arg_list_world[] = {"./bin/world", NULL};
  pid_world = spawn("./bin/world", arg_list_world);
  if (pid_world == -1)
  {
    // Go to spawn_err if spawn() returns -1
    goto spawn_err;
  }

  // Inspection console process
  char *arg_list_inspection[] = {"/usr/bin/konsole", "-e", "./bin/inspection", pid_mx_str, pid_mz_str, NULL};
  pid_insp = spawn("/usr/bin/konsole", arg_list_inspection);
  if (pid_insp == -1)
  {
    // Go to spawn_err if spawn() returns -1
    goto spawn_err;
  }

  // If no error occured, log that all processes have been started
  t = time(NULL);
  tm = *localtime(&t);
  sprintf(buffer, "%d-%d-%d %d:%d:%d: <master_process> All processes started\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  m = write(log_fd, buffer, strlen(buffer));
  if (m == -1 || m != strlen(buffer))
  {
    // If error orccurs while writing to log file, print error message and exit
    perror("Error writing to log file");
    close(log_fd);
    return 1;
  }

  // If no error occured, go to no_err label
  goto no_err;

// If an error occurred while spawning a process
spawn_err:
  // Print an error message and exit
  perror("Error spawning process");
  // Close the log file
  close(log_fd);
  // Kill all the child processes
  kill_all();
  return 1;

// If no error occured
no_err:

  // Create all the log files
  int ret = create_log_files();
  if (ret == -1)
  {
    // If an error occured while creating the log files, print an error message and exit
    perror("Error creating log files");
    // Close the log file
    close(log_fd);
    // Kill all the child processes
    kill_all();
    return 1;
  }

  // Call the watchdog function
  ret = watchdog();

  // No need to wait for child processes to terminate as they have already been killed by the watchdog function

  // If watchdog() returns 0, the processes have been terminated for inactivity
  if (ret == 0)
  {
    // Log that all processes have been terminated
    t = time(NULL);
    tm = *localtime(&t);
    sprintf(buffer, "%d-%d-%d %d:%d:%d: <master_process> All processes terminated for inactivity\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    m = write(log_fd, buffer, strlen(buffer));
    if (m == -1 || m != strlen(buffer))
    {
      // If error orccurs while writing to log file, print error message and exit
      perror("Error writing to log file");
      close(log_fd);
      return 1;
    }
  }

  // If watchdog() returns 1, an error occured
  if (ret == 1)
  {
    // Log that an error occurred
    t = time(NULL);
    tm = *localtime(&t);
    sprintf(buffer, "%d-%d-%d %d:%d:%d: <master_process> Error in watchdog: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, strerror(errno));
    m = write(log_fd, buffer, strlen(buffer));
    if (m == -1 || m != strlen(buffer))
    {
      // If error orccurs while writing to log file, print error message and exit
      perror("Error writing to log file");
      close(log_fd);
      return 1;
    }
    // Print an error message and exit
    printf("An error occured, check log file for details\n");
    fflush(stdout);
    // Close the log file
    close(log_fd);
    return 1;
  }

  // If watchdog() returns -1, a child process terminated unexpectedly
  if (ret == -1)
  {
    // Log that a child process terminated unexpectedly
    t = time(NULL);
    tm = *localtime(&t);
    sprintf(buffer, "%d-%d-%d %d:%d:%d: <master_process> Child terminated unexpectedly\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    m = write(log_fd, buffer, strlen(buffer));
    if (m == -1 || m != strlen(buffer))
    {
      // If error orccurs while writing to log file, print error message and exit
      perror("Error writing to log file");
      close(log_fd);
      return 1;
    }

    // If status is 1, the child process terminated for a system call error
    if (WEXITSTATUS(status) == 1)
    {
      // Print an error message and exit
      printf("Child terminated for a system call error. Check log files for more details.\n");
      fflush(stdout);
    }
    else
    {
      // If status is different than 1, the child process terminated for log file error
      // and the status variable will contain the error code returned by the child process

      // Print an error message and exit
      printf("Child process terminated for log file error: %s\n", strerror(WEXITSTATUS(status)));
      fflush(stdout);
    }
  }

  // Log the end of the program
  t = time(NULL);
  tm = *localtime(&t);
  sprintf(buffer, "%d-%d-%d %d:%d:%d: <master_process> Master process terminated\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  m = write(log_fd, buffer, strlen(buffer));

  // Check for errors
  if (m == -1 || m != strlen(buffer))
  {
    // If an error occurred, print an error message and exit
    perror("Error writing to log file");
    // Close the log file
    close(log_fd);
    return 1;
  }

  // Close the log file
  close(log_fd);

  return 0;
}