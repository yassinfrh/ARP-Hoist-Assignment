#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

// Define maximum and minimum positions
const float min_x_pos = 0.0;
const float max_x_pos = 40.0;
const float min_z_pos = 0.0;
const float max_z_pos = 10.0;

// Buffer to store the log message
char log_buffer[100];

// File descriptor for log file
int log_fd;

// Variable to store the error
volatile int error = 0;

// Function to write on log file
int write_log(char *to_write, char type)
{
    // Log that in command_console process button has been pressed with date and time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // If type is 'e' write error
    if (type == 'e')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <world_process> error: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 'b' write button pressed
    else if (type == 'i')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <world_process> Position: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }

    // Write on log file
    int m = write(log_fd, log_buffer, strlen(log_buffer));

    // Check for errors
    if (m == -1 || m != strlen(log_buffer))
    {
        return 2;
    }

    return 0;
}

// Function to return maximum of two integers
int max(int a, int b)
{
    return (a > b) ? a : b;
}

// Function to randomly get a number between two integers
int random_between(int a, int b)
{
    return (rand() % (b - a + 1)) + a;
}

// Function to pick a random number between two integers
int pick_random(int a, int b)
{
    int num = random_between(a, b);

    // If num is closer to a, return a
    if (num - a < b - num)
    {
        return a;
    }
    // If num is closer to b, return b
    else
    {
        return b;
    }
}

// Function to add a random 0.5% error to the position
float add_error(float pos)
{
    // Calculate the error
    float error = pos * 0.005;

    // Calculate the minimum and maximum values
    float min = pos - error;
    float max = pos + error;

    // Return a random value between the minimum and maximum
    return (float)random_between((int)(min * 100), (int)(max * 100)) / 100;
}

// Function to read and return the real position
float read_real_pos(int *fd, char axis)
{
    // Variable to store the position
    char pos[10];

    // Read the position from the file
    if (read(*fd, pos, 10) == -1)
    {
        // If error return -1.0
        return -1.0;
    }

    // Store the value in the real position variable and add a random 0.5% error
    float real_pos = add_error(atof(pos));

    // Check if the position is out of bounds
    if (axis == 'x')
    {
        if (real_pos < min_x_pos)
        {
            real_pos = min_x_pos;
        }
        else if (real_pos > max_x_pos)
        {
            real_pos = max_x_pos;
        }
    }
    else if (axis == 'z')
    {
        if (real_pos < min_z_pos)
        {
            real_pos = min_z_pos;
        }
        else if (real_pos > max_z_pos)
        {
            real_pos = max_z_pos;
        }
    }

    // Return the position
    return real_pos;
}

int main(int argc, char const *argv[])
{
    // Open log file
    if ((log_fd = open("log/world.log", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1)
    {
        // If error occurs while opening the log file
        exit(errno);
    }

    // Variables to store the real values of the positions
    float real_x_pos = 0.0;
    float real_z_pos = 0.0;

    // File descriptors
    int fdx_pos, fdz_pos, fd_real_pos;

    // FIFOs locations
    char *x_pos_fifo = "/tmp/x_pos_fifo";
    char *z_pos_fifo = "/tmp/z_pos_fifo";
    char *real_pos_fifo = "/tmp/real_pos_fifo";

    // Create the FIFOs
    mkfifo(x_pos_fifo, 0666);
    mkfifo(z_pos_fifo, 0666);
    mkfifo(real_pos_fifo, 0666);

    // Open the FIFOs
    if ((fdx_pos = open(x_pos_fifo, O_RDWR)) == -1) // Opening in O_RDWR mode to avoid receiving EOF
    {
        // If error occurs while opening the FIFO
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

    if ((fdz_pos = open(z_pos_fifo, O_RDWR)) == -1) // Opening in O_RDWR mode to avoid receiving EOF
    {
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the file descriptors
        close(fdx_pos);
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }

    if ((fd_real_pos = open(real_pos_fifo, O_WRONLY)) == -1)
    {
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the file descriptors
        close(fdx_pos);
        close(fdz_pos);
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }

    // Variable to store the number of loops
    int loops = 0;

    // Infinite loop
    while (1)
    {
        // Variable to store the read values
        char x_pos[10];
        char z_pos[10];

        // Setting parameters for select function
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fdx_pos, &readfds);
        FD_SET(fdz_pos, &readfds);
        int max_fd = max(fdx_pos, fdz_pos) + 1;

        // Setting timeout for select function
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;

        // Wait for the file descriptor to be ready
        int ready = select(max_fd, &readfds, NULL, NULL, &timeout);

        // Check if the file descriptor is ready
        if (ready < 0)
        {
            // If error occurs while waiting for the file descriptor
            error = 1;
            break;
        }
        else if (ready > 0)
        {
            // Increment loops
            loops++;

            // If both file descriptors are ready
            if (FD_ISSET(fdx_pos, &readfds) && FD_ISSET(fdz_pos, &readfds))
            {
                // Randomly select one of the file descriptors
                if (pick_random(fdx_pos, fdz_pos) == fdx_pos)
                {
                    // Read and store the value in the x position variable
                    real_x_pos = read_real_pos(&fdx_pos, 'x');
                }
                else
                {
                    // Read and store the value in the z position variable
                    real_z_pos = read_real_pos(&fdz_pos, 'z');
                }
            }
            // If only the x position file descriptor is ready
            else if (FD_ISSET(fdx_pos, &readfds))
            {
                // Read and store the value in the x position variable
                real_x_pos = read_real_pos(&fdx_pos, 'x');
            }
            // If only the z position file descriptor is ready
            else if (FD_ISSET(fdz_pos, &readfds))
            {
                // Read and store the value in the z position variable
                real_z_pos = read_real_pos(&fdz_pos, 'z');
            }

            // If error occurs while reading the position
            if (real_x_pos == -1.0 || real_z_pos == -1.0)
            {
                error = 1;
                break;
            }

            // Create a string to store the real position with the format "x_pos;z_pos"
            char real_pos[20];
            sprintf(real_pos, "%f;%f", real_x_pos, real_z_pos);

            // Log every 10 loops
            if (loops == 10)
            {
                if (error = write_log(real_pos, 'i'))
                {
                    // If error occurs while writing on log file
                    break;
                }
                loops = 0;
            }

            // Write the real position to the FIFO
            int m = write(fd_real_pos, real_pos, strlen(real_pos));
            if (m == -1 || m != strlen(real_pos))
            {
                // If error occurs while writing on the FIFO
                error = 1;
                break;
            }
        }
    }

    // Close the FIFOs
    close(fdx_pos);
    close(fdz_pos);
    close(fd_real_pos);

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

    if (error == 2)
    {
        // If error occurs while writing on log file
        exit(errno);
    }

    exit(0);
}
