/*

This program connects to a server, waits for ShowFlakes TCP signals and controlls GPIO outpurt accordingly.

*/

/* -------- INCLUDE LIBRARIES -------- */
// Basic
# include <stdio.h>

// For the GPIO
# include <math.h>
# include <sys/time.h>
# include <bcm2835.h>

// For the TCP Client
# include <netdb.h> 
# include <stdio.h> 
# include <stdlib.h> 
# include <string.h> 
# include <sys/socket.h>
# include <sys/ioctl.h>

/* -------- DEFINITIONS -------- */
# define VERSION "v1.0"

// GPIO
# define FPS 60
# define UPS 25

# define C01 RPI_V2_GPIO_P1_03
# define C02 RPI_V2_GPIO_P1_05
# define C03 RPI_V2_GPIO_P1_07
# define C04 RPI_V2_GPIO_P1_08
# define C05 RPI_V2_GPIO_P1_10
# define C06 RPI_V2_GPIO_P1_11
# define C07 RPI_V2_GPIO_P1_12
# define C08 RPI_V2_GPIO_P1_13
# define C09 RPI_V2_GPIO_P1_15
# define C10 RPI_V2_GPIO_P1_16
# define C11 RPI_V2_GPIO_P1_18
# define C12 RPI_V2_GPIO_P1_19
# define C13 RPI_V2_GPIO_P1_21
# define C14 RPI_V2_GPIO_P1_22
# define C15 RPI_V2_GPIO_P1_23
# define C16 RPI_V2_GPIO_P1_24
# define C17 RPI_V2_GPIO_P1_26

// TCP client
# define HOST "192.168.1.12"
# define PORT 5466
# define MAX_RECV 100
# define RELAY_NAME "SFR-01"
# define SERVER_CHECK_INTERVAL 5

/* -------- FUNCTION HEADERS -------- */

int SetupGPIO();
int RunFrame();

int SetupSocket();
int ConnectToServer();
void ReConnectToServer();
void GetChannelUpdates();

void MainLoop();

/* -------- GLOBAL VARIABLES -------- */

// GPIO
int channel_patch[] = {C01, C02, C03, C04, C05, C06, C07, C08, C09, C10, C11, C12, C13, C14, C15, C16, C17};	// GPIO PIN numbers for each channel
int channel_levels[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};															// Levels (0 - 255) of each channel
int channel_states[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};														// The state of each channel (1 = HIGH, 0 = LOW)

// TCP Client
int socket_id;
struct sockaddr_in server_address;
int server_check_interval_count = 0;
int server_check_status = 0;
int connection_alive = 0;

/* ------- MAIN PROGRAM -------- */

int main(void)
{
	// Print welcome message
	printf("Show Flakes TCP Relay %s\n", VERSION);
	
	// Setup the GPIO
	if (!SetupGPIO())
	{
		return (1);
	}
	
	// Setup the TCP Client
	if (!SetupSocket())
	{
		return (1);
	}
	
	if (!ConnectToServer())
	{
		//return (1);
	}
	
	// Start the main loop
	MainLoop();
	
	return (0);
}

void MainLoop()
{
	// Setup variables
	struct timeval time_now, frame_start_time, update_start_time;	// Used for timing

	unsigned long micro_per_frame = ((double) 1 / FPS) * 1000000;
	unsigned long micro_per_update = ((double) 1 / UPS) * 1000000;
	
	unsigned long frame_micro_time_passed, update_micro_time_passed;
	float frame_progress;

	gettimeofday(&frame_start_time, NULL);	// Set the start time
	gettimeofday(&update_start_time, NULL);	// Set the start time

	while (1)
	{
		// Calculate the time passed and the frame progress
		gettimeofday(&time_now, NULL);
		
		frame_micro_time_passed = ((time_now.tv_sec - frame_start_time.tv_sec) * 1000000) + (time_now.tv_usec - frame_start_time.tv_usec);
		update_micro_time_passed = ((time_now.tv_sec - update_start_time.tv_sec) * 1000000) + (time_now.tv_usec - update_start_time.tv_usec);
		
		frame_progress = (double) frame_micro_time_passed / micro_per_frame;

		// Loop through each channel and set outputs accordingly
		for (int c = 0; c < 17; c++)
		{	
			if (channel_states[c] == 1 && (int) (frame_progress * 255) > (255 - channel_levels[c]))
			{
				channel_states[c] = 0;
				bcm2835_gpio_write(channel_patch[c], LOW);
			}
		}

		// Update the frame
		if (frame_progress >= 1)
		{
			// Set all outputs HIGH if not already
			for (int i = 0; i < 17; i++)
			{
				if(channel_states[i] == 0)
				{
					bcm2835_gpio_write(channel_patch[i], HIGH);
					channel_states[i] = 1;
				}
			}

			// Set the start time
			gettimeofday(&frame_start_time, NULL);
		}
		
		// Update
		if (update_micro_time_passed >= micro_per_update)
		{
			// Update the levels
			GetChannelUpdates();
			
			// Set the start time
			gettimeofday(&update_start_time, NULL);
		}

	}
}

int SetupGPIO()
{
	// INIT the module
	if (!bcm2835_init())
	{
		printf("[CTR] Failed to INIT bcm2835!\n");
		return (0);
	}
	else
	{
		printf("[CTR] GPIO INIT compleate!\n");
	}
	
	// Set the GPIO to output and high
	
	for (int i = 0; i < 17; i++)
	{
		bcm2835_gpio_fsel(channel_patch[i], BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_write(channel_patch[i], HIGH);
		channel_states[i] = 1;
	}
	
	return (1);
}

int SetupSocket()
{
	printf("[NET] Createing Socket...\n");
	
	int sockfd, connfd; 
	struct sockaddr_in servaddr, cli; 

	// Create the Socket
	socket_id = socket(AF_INET, SOCK_STREAM, 0);
	
	// Check the socket was created corectly
	if (socket_id == -1)
	{ 
		printf("[NET] Failed to create the Socket!\n"); 
		return (0);
	}
	else
	{
		printf("[NET] Socket created!\n");
	}
	
	// Setup the IP address
	server_address.sin_family = AF_INET; 
	server_address.sin_addr.s_addr = inet_addr(HOST); 
	server_address.sin_port = htons(PORT); 
}

int ConnectToServer()
{	
	printf("[NET] Connecting to %s:%i...\n", HOST, PORT);
	
	
	// Connect to the server
	if (connect(socket_id, (struct sockaddr * ) &server_address, sizeof(server_address)) != 0)
	{ 
		printf("[NET] Failed to connect to the Server!\n"); 
		return (0);
	} 
	else
	{
		printf("[NET] Connected!\n"); 
		connection_alive = 1;
	}
	
	// Send Hand Shake info
	char handshake_header[] = RELAY_NAME;
	write(socket_id, handshake_header, sizeof(handshake_header));
	
	return (1);
}

void GetChannelUpdates()
{	
	// Read two bytes at a time
	char buffer[2];
	int channel, level, bytes_read, bytes_available;
	
	ioctl(socket_id, FIONREAD, &bytes_available);
	
	while(bytes_available > 0)
	{
		// Read the bytes
		bytes_read = recv(socket_id, &buffer, 2, 0);
		
		if (bytes_read < 2)
		{
			break;
		}

		server_check_status = 0;
		
		// Get the channel and level
		channel = (int) buffer[0];
		level = (int) buffer[1];
		
		// Check if the channel is in the range
		if (0 <= channel <= 16)
		{
			channel_levels[channel] = level;	// Set the new level
		}
		
		ioctl(socket_id, FIONREAD, &bytes_available);
	}

	// Check the client is still connected to the server
	server_check_interval_count += 1;

	if (server_check_interval_count > UPS * SERVER_CHECK_INTERVAL)
	{
		server_check_interval_count = 0;

		if (server_check_status == 0)	// Send a ping to the server
		{
			server_check_status = 1;

			if (connection_alive)
			{
				char server_ping[] = "PING";
				write(socket_id, server_ping, sizeof(server_ping));
			}
		}
		else	// Failed to ping Server
		{
			connection_alive = 0;
			printf("Warning: Failed to PING Server. Reconnecting...\n");
			ReConnectToServer();
			server_check_status = 0;
		}
	}
}

void ReConnectToServer()
{
	shutdown(socket_id, 2);

	SetupSocket();
	ConnectToServer();
}
