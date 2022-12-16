#include "./../include/inspection_utilities.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

// File descriptor for the log file
int log_fd;

// Variable to store the error
volatile int error = 0;

// Function to write on log file errors or button pressed
int write_log(char *to_write, char type)
{
    // Log that in command_console process button has been pressed or error with date and time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char buffer[100];

    // If type is 'e' write error
    if (type == 'e')
    {
        sprintf(buffer, "%d-%d-%d %d:%d:%d: <inspection_process> error: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 'b' write button pressed
    else if (type == 'b')
    {
        sprintf(buffer, "%d-%d-%d %d:%d:%d: <inspection_process> button pressed: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }

    // Write on log file
    int m = write(log_fd, buffer, strlen(buffer));

    // Check for errors
    if (m == -1 || m != strlen(buffer))
    {
        return 2;
    }

    return 0;
}

int main(int argc, char const *argv[])
{
    // Mx and Mz process ids
    pid_t pid_mx = atoi(argv[1]);
    pid_t pid_mz = atoi(argv[2]);

    // Open log file
    if ((log_fd = open("log/inspection.log", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1)
    {
        // If error occurs while opening log file
        exit(errno);
    }

    // File descriptor
    int fd_real_pos;

    // FIFO location
    char *real_pos_fifo = "/tmp/real_pos_fifo";

    // Create the FIFO
    mkfifo(real_pos_fifo, 0666);

    // Open the FIFO
    if ((fd_real_pos = open(real_pos_fifo, O_RDWR)) == -1)
    {
        // If error occurs while opening FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close log file
        close(log_fd);
        if(ret){
            // If error occurs while writing on log file
            exit(errno);
        }

        exit(1);
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // End-effector coordinates
    float ee_x = 0.0;
    float ee_z = 0.0;

    // Initialize User Interface
    init_console_ui();

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

                // STOP button pressed
                if (check_button_pressed(stp_button, &event))
                {
                    // Send stop signal to mx and mz
                    if(kill(pid_mx, SIGUSR1) || kill(pid_mz, SIGUSR1)){
                        // If error occurs while sending signal
                        error = 1;
                        break;
                    }

                    // Log the pressed button
                    if(error = write_log("STOP", 'b')){
                        // If error occurs while writing on log file
                        break;
                    }
                }

                // RESET button pressed
                else if (check_button_pressed(rst_button, &event))
                {
                    // Send reset signal to mx and mz
                    if(kill(pid_mx, SIGUSR2) || kill(pid_mz, SIGUSR2)){
                        // If error occurs while sending signal
                        error = 1;
                        break;
                    }

                    // Log the pressed button
                    if(error = write_log("RESET", 'b')){
                        // If error occurs while writing on log file
                        break;
                    }
                }
            }
        }

        // Variables to store the real values of the positions
        char real_pos_str[20];

        // Setting parameters for select function
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(fd_real_pos, &readfds);
        int max_fd = fd_real_pos + 1;

        // Setting timeout for select function
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        // Wait for the file descriptor to be ready
        int ready = select(max_fd, &readfds, NULL, NULL, &timeout);

        // Check if the file descriptor is ready
        if (ready < 0 && errno != EINTR) // errno != EINTR is to avoid the select function to be interrupted by resize signal
        {
            // If error occurs while waiting for the file descriptor
            error = 1;
            break;
        }
        else if (ready > 0)
        {
            // Read the real position
            if (read(fd_real_pos, real_pos_str, 20) == -1)
            {
                // If error occurs while reading the real position
                error = 1;
                break;
            }

            // When pressing the reset button, sometimes the read x_pos value is wrong
            // If this happens, I set the x position to the previous value
            float old_x = ee_x;

            // Store the x and z position from the string with format "x;z"
            sscanf(real_pos_str, "%f;%f", &ee_x, &ee_z);

            if(ee_x > 40.0)
            {
                ee_x = old_x;
            }
        }

        // Update UI
        update_console_ui(&ee_x, &ee_z);
    }

    // Close the FIFO
    close(fd_real_pos);

    // Terminate
    endwin();

    if(error == 1){
        // If error occurs while sending signal or reading the real position
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close log file
        close(log_fd);
        if(ret){
            // If error occurs while writing on log file
            exit(errno);
        }

        exit(1);
    }

    // Close log file
    close(log_fd);

    if(error == 2){
        // If error occurs while writing on log file
        exit(errno);
    }

    exit(0);
}
