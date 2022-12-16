#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

// Flag to check if stop or reset handlers were called
int stop_flag = 0;
int reset_flag = 0;

// Constants to store the minimum and maximum x position
const float x_min = 0.0;
const float x_max = 40.0;

// File descriptor for the log file
int log_fd;

// File descriptors for pipes
int fd_vx, fdx_pos;

// Variables to store position and velocity
float x_pos = 0.0;
int vx = 0;

// Buffer to store the log message
char log_buffer[100];

// Variable to store the errors
// 0 = no error
// 1 = system call error
// 2 = error while writing on log file
volatile int error = 0;

// Signal handlers
void reset_handler(int signo);
void stop_handler(int signo);

// Function to write on log
int write_log(char *to_write, char type)
{
    // Log that in command_console process button has been pressed with date and time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // If type is 'e' then it is an error
    if (type == 'e')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mx_process> error: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 'i' then it is an info
    else if (type == 'i')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mx_process> new speed: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 's' then it is a signal
    else if (type == 's')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mx_process> signal received: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }

    int m = write(log_fd, log_buffer, strlen(log_buffer));
    // Check for errors
    if (m == -1 || m != strlen(log_buffer))
    {
        return 2;
    }

    return 0;
}

// Stop signal handler
void stop_handler(int signo)
{
    if (signo == SIGUSR1)
    {
        // Setting stop_flag to true
        stop_flag = 1;

        // Log that the process has received a signal
        if (error = write_log("STOP", 's'))
        {
            return;
        }

        // Stop the motor
        vx = 0;

        // Listen for the next signal
        if (signal(SIGUSR1, stop_handler) == SIG_ERR || signal(SIGUSR2, reset_handler) == SIG_ERR)
        {
            // If error occurs, set handler_error to 1
            error = 1;
            return;
        }
    }
}

// Reset signal handler
void reset_handler(int signo)
{
    if (signo == SIGUSR2)
    {
        // Setting stop_flag to false
        stop_flag = 0;

        // Setting reset_flag to true
        reset_flag = 1;

        // Log that the process has received a signal
        if (error = write_log("RESET", 's'))
        {
            // If log fails, return
            return;
        }

        // Variable to count the loops
        int loops = 0;

        // Setting velocity to -4
        vx = -4;

        // Listen for stop signal
        if (signal(SIGUSR1, stop_handler) == SIG_ERR)
        {
            // If error occurs, set handler_error to 1
            error = 1;
            return;
        }

        // Looping for 5 seconds
        while (loops < 10)
        {
            // Setting up the select to read from the pipe and ignore the read values
            // Set the file descriptors to be monitored
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd_vx, &readfds);

            // Set the timeout
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;

            // Wait for the file descriptor to be ready
            int ready = select(fd_vx + 1, &readfds, NULL, NULL, &timeout);

            // Check if the file descriptor is ready
            if (ready > 0)
            {
                // Read the velocity increment
                char buffer[2];
                if (read(fd_vx, buffer, 2) == -1)
                {
                    // If error occurs, set handler_error to 1
                    error = 1;
                    return;
                }
            }
            // Error handling
            else if (ready < 0 && errno != EINTR)
            {
                // If error occurs, set handler_error to 1
                error = 1;
                return;
            }

            // If reset was interrupted by a stop, exit the handler
            if (stop_flag)
            {
                return;
            }

            // Updating position
            x_pos += vx;

            // If position is lower than 0, make it 0
            if (x_pos < 0)
            {
                x_pos = 0;
            }

            // Writing position to pipe
            char x_pos_str[10];
            sprintf(x_pos_str, "%f", x_pos);

            int m = write(fdx_pos, x_pos_str, strlen(x_pos_str) + 1);
            if (m == -1 || m != strlen(x_pos_str) + 1)
            {
                // If error occurs, set handler_error to 1
                error = 1;
                return;
            }

            // Increment loops
            loops++;
        }

        // Set velocity to 0
        vx = 0;

        // Listen for the next signal
        if (signal(SIGUSR1, stop_handler) == SIG_ERR || signal(SIGUSR2, reset_handler) == SIG_ERR)
        {
            // If error occurs, set handler_error to 1
            error = 1;
            return;
        }
    }
}

int main(int argc, char const *argv[])
{
    // Open the log file
    if ((log_fd = open("log/mx.log", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1)
    {
        // If error occurs while opening the log file
        exit(errno);
    }

    // Create the FIFOs
    char *vx_fifo = "/tmp/vx_fifo";
    char *x_pos_fifo = "/tmp/x_pos_fifo";
    mkfifo(vx_fifo, 0666);
    mkfifo(x_pos_fifo, 0666);

    // Open the FIFOs
    if ((fd_vx = open(vx_fifo, O_RDWR)) == -1) // O_RDWR is needed to avoid receiving EOF in select
    {
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the log file
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing to the log file
            exit(errno);
        }

        exit(1);
    }

    if ((fdx_pos = open(x_pos_fifo, O_WRONLY)) == -1)
    {
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close file descriptors
        close(fd_vx);
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing to the log file
            exit(errno);
        }

        exit(1);
    }

    // Listen for signals
    if (signal(SIGUSR1, stop_handler) == SIG_ERR || signal(SIGUSR2, reset_handler) == SIG_ERR)
    {
        // If error occurs while setting the signal handlers
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close file descriptors
        close(fd_vx);
        close(fdx_pos);
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing to the log file
            exit(errno);
        }

        exit(1);
    }

    // Loop until handler_error is set to true
    while (!error)
    {
        // Setting signal falgs to false
        stop_flag = 0;
        reset_flag = 0;

        // Boolean to check if the position has changed
        int pos_changed = 0;

        // Set the file descriptors to be monitored
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_vx, &readfds);

        // Set the timeout
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        // Wait for the file descriptor to be ready
        int ready = select(fd_vx + 1, &readfds, NULL, NULL, &timeout);

        // Check if the file descriptor is ready
        if (ready > 0)
        {
            // Read the velocity increment
            char buffer[2];
            if (read(fd_vx, buffer, 2) == -1)
            {
                // If error occurs while reading from the FIFO
                error = 1;
                break;
            }

            int vx_increment = atoi(buffer);

            // Stop motor
            if (vx_increment == 0 && vx != 0)
            {
                vx = 0;

                // Log that the motor has been stopped
                char to_write[4];
                sprintf(to_write, "%d", vx);
                if (error = write_log(to_write, 'i'))
                {
                    // If error occurs while writing to the log file
                    break;
                }
            }

            // Ignore the velocity increment or decrement if the position is at the limit
            else if ((vx_increment == 1 && x_pos < x_max) || (vx_increment == 2 && x_pos > x_min))
            {
                if (vx_increment == 2)
                {
                    vx_increment = -1;
                }

                // Increment the velocity
                vx += vx_increment;

                // Log the new velocity
                char to_write[4];
                sprintf(to_write, "%d", vx);
                if (error = write_log(to_write, 'i'))
                {
                    // If error occurs while writing to the log file
                    break;
                }
            }
        }
        // Error handling
        else if (ready < 0 && errno != EINTR)
        {
            // If error occurs while waiting for the file descriptor to be ready
            error = 1;
            break;
        }

        // Update the position
        float pos_increment = vx * 0.5;
        float new_x_pos = x_pos + pos_increment;

        // Check if the position is out of bounds
        if (new_x_pos < x_min)
        {
            new_x_pos = x_min;

            // Stop the motor
            vx = 0;
        }
        else if (new_x_pos > x_max)
        {
            new_x_pos = x_max;

            // Stop the motor
            vx = 0;
        }

        // Check if the position has changed
        if (new_x_pos != x_pos)
        {
            // Update the position
            x_pos = new_x_pos;
            pos_changed = 1;
        }

        // Write the position to the FIFO if it has changed
        if (pos_changed)
        {
            // Write the position
            char x_pos_str[10];
            sprintf(x_pos_str, "%f", x_pos);

            // If reset signal was received,
            if (reset_flag)
            {
                vx = 0;
                x_pos = 0.0;
                // Skip writing to the FIFO
                continue;
            }

            // If stop signal was received
            if (stop_flag)
            {
                vx = 0;
                // Skip writing to the FIFO
                continue;
            }

            int m = write(fdx_pos, x_pos_str, strlen(x_pos_str) + 1);
            if (m == -1 || m != strlen(x_pos_str) + 1)
            {
                // If error occurs while writing to the FIFO
                error = 1;
                break;
            }
        }
    }

    // Close the FIFOs
    close(fd_vx);
    close(fdx_pos);

    if (error == 1)
    {
        // If error occurs while reading the position
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the log file
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }
    
    // Close the log file
    close(log_fd);

    if(error == 2){
        // If error occurs while writing on log file
        exit(errno);
    }

    exit(0);
}
