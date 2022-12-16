#include "./../include/command_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

// Buffer to store the log message
char log_buffer[100];

// File descriptor for the log file
int log_fd;

// Function to write on log file the pressed button
int write_log(char *to_write, char type)
{
    // Log that in command_console process button has been pressed with date and time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    if (type == 'b')
    {
        // If button message
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <command_process> Button %s pressed\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    else if (type == 'e')
    {
        // If error message
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <command_process> Error: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    int m = write(log_fd, log_buffer, strlen(log_buffer));

    // Check for errors
    if (m == -1 || m != strlen(log_buffer))
    {
        return 2;
    }

    return 0;
}

// Function to send velocity to motor
int send_velocity(int *fd, int v)
{
    // Send velocity to pipe fd
    char buffer[2];
    sprintf(buffer, "%d", v);
    int m = write(*fd, buffer, strlen(buffer));
    if (m == -1 || m != strlen(buffer))
    {
        // Log the error
        if (write_log(strerror(errno), 'e') == 2)
        {
            // If error accured while writing on log file
            return 2;
        }

        return 1;
    }

    return 0;
}

int main(int argc, char const *argv[])
{
    // Open the log file
    if ((log_fd = open("log/command.log", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1)
    {
        // If error accured while opening the log file
        exit(errno);
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize User Interface
    init_console_ui();

    // Create the FIFOs
    char *vx_fifo = "/tmp/vx_fifo";
    char *vz_fifo = "/tmp/vz_fifo";
    mkfifo(vx_fifo, 0666);
    mkfifo(vz_fifo, 0666);

    // File descriptors
    int fd_vx, fd_vz;

    // Open the FIFOs
    if ((fd_vx = open(vx_fifo, O_WRONLY)) == -1)
    {
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the log file
        close(log_fd);
        // If error accured while writing on log file
        if (ret)
        {
            exit(errno);
        }

        exit(1);
    }

    if ((fd_vz = open(vz_fifo, O_WRONLY)) == -1)
    {
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close file descriptors
        close(log_fd);
        close(fd_vx);
        // If error accured while writing on log file
        if (ret)
        {
            exit(errno);
        }

        exit(1);
    }

    // Variable to store the error code
    int err = 0;

    // Infinite loop
    while (TRUE)
    {
        // Get mouse/resize commands in non-blocking mode...
        int cmd = getch();

        // If user resizes screen, re-draw UI
        if (cmd == KEY_RESIZE)
        {
            if (first_resize)
            {
                first_resize = FALSE;
            }
            else
            {
                reset_console_ui();
            }
        }
        // Else if mouse has been pressed
        else if (cmd == KEY_MOUSE)
        {

            // Check which button has been pressed...
            if (getmouse(&event) == OK)
            {

                // Vx++ button pressed
                if (check_button_pressed(vx_incr_btn, &event))
                {
                    // Log button pressed
                    if (err = write_log("Vx++", 'b'))
                    {
                        // If error accured while writing on log file
                        break;
                    }

                    // Send velocity to motor x
                    if (err = send_velocity(&fd_vx, 1))
                    {
                        // If error accured while sending velocity
                        break;
                    }
                }

                // Vx-- button pressed
                else if (check_button_pressed(vx_decr_btn, &event))
                {
                    // Log button pressed
                    if (err = write_log("Vx--", 'b'))
                    {
                        // If error accured while writing on log file
                        break;
                    }

                    // Send velocity to motor x
                    if (err = send_velocity(&fd_vx, 2))
                    { // 2 is the code for Vx--
                        // If error accured while sending velocity
                        break;
                    }
                }

                // Vx stop button pressed
                else if (check_button_pressed(vx_stp_button, &event))
                {
                    // Log button pressed
                    if (err = write_log("Vx stop", 'b'))
                    {
                        // If error accured while writing on log file
                        break;
                    }

                    // Send velocity to motor x
                    if (err = send_velocity(&fd_vx, 0))
                    {
                        // If error accured while sending velocity
                        break;
                    }
                }

                // Vz++ button pressed
                else if (check_button_pressed(vz_incr_btn, &event))
                {
                    // Log button pressed
                    if (err = write_log("Vz++", 'b'))
                    {
                        // If error accured while writing on log file
                        break;
                    }

                    // Send velocity to motor z
                    if (err = send_velocity(&fd_vz, 1))
                    {
                        // If error accured while sending velocity
                        break;
                    }
                }

                // Vz-- button pressed
                else if (check_button_pressed(vz_decr_btn, &event))
                {
                    // Log button pressed
                    if (err = write_log("Vz--", 'b'))
                    {
                        // If error accured while writing on log file
                        break;
                    }

                    // Send velocity to motor z
                    if (err = send_velocity(&fd_vz, 2))
                    { // 2 is the code for Vz--
                        // If error accured while sending velocity
                        break;
                    }
                }

                // Vz stop button pressed
                else if (check_button_pressed(vz_stp_button, &event))
                {
                    // Log button pressed
                    if (err = write_log("Vz stop", 'b'))
                    {
                        // If error accured while writing on log file
                        break;
                    }

                    // Send velocity to motor z
                    if (err = send_velocity(&fd_vz, 0))
                    {
                        // If error accured while sending velocity
                        break;
                    }
                }
            }
        }
        refresh();
    }

    // Close the FIFOs
    close(fd_vx);
    close(fd_vz);

    // Terminate
    endwin();

    if (err == 1)
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

    if(err == 2){
        // If error occurs while writing on log file
        exit(errno);
    }

    exit(0);
}