#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#define BOARD_SIZE 10
#define SHM_SIZE BOARD_SIZE *BOARD_SIZE * sizeof(int)

int *shm_ptr;
int shm_id;

typedef struct
{
    int row;
    int col;
} Coordinate;

void print_board()
{
    int i, j;
    printf("    A  B  C  D  E  F  G  H  I  J \n");
    for (i = 0; i < BOARD_SIZE; i++)
    {
        printf(" %d ", i + 1);
        for (j = 0; j < BOARD_SIZE; j++)
        {
            int val = *(shm_ptr + i * BOARD_SIZE + j);
            if (val == -1)
                printf(" ~ ");
            else if (val == 0)
                printf(" O ");
            else if (val == 1)
                printf(" X ");
        }
        printf("\n");
    }
}

void print_ships() {
    int i, j;
    printf("    A  B  C  D  E  F  G  H  I  J \n");
    for (i = 0; i < BOARD_SIZE; i++) {
        printf(" %d ", i + 1);
        for (j = 0; j < BOARD_SIZE; j++) {
            int val = *(shm_ptr + i * BOARD_SIZE + j);
            if (val == -1)
                printf(" ~ ");
            else
                printf(" X ");
        }
        printf("\n");
    }
}

void initialize_board()
{
    int i;
    for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
    {
        *(shm_ptr + i) = -1;
    }
}

int is_valid_coordinate(Coordinate coord)
{
    if (coord.row >= 0 && coord.row < BOARD_SIZE && coord.col >= 0 && coord.col < BOARD_SIZE)
    {
        return 1;
    }
    return 0;
}

int is_empty_coordinate(Coordinate coord)
{
    int val = *(shm_ptr + coord.row * BOARD_SIZE + coord.col);
    if (val == -1)
        return 1;
    return 0;
}

void set_ship(Coordinate coord, int player_id)
{
    *(shm_ptr + coord.row * BOARD_SIZE + coord.col) = player_id;
}

int is_ship_hit(Coordinate coord, int player_id)
{
    int val = *(shm_ptr + coord.row * BOARD_SIZE + coord.col);
    if (val >= 0 && val != player_id)
    {
        *(shm_ptr + coord.row * BOARD_SIZE + coord.col) = 0;
        return 1;
    }
    return 0;
}

void handle_sigint(int sig)
{
    shmdt(shm_ptr);
    shmctl(shm_id, IPC_RMID, NULL);
    printf("\nServer shutting down.\n");
    exit(0);
}

int main()
{
    // Set up shared memory
    key_t key = ftok("shmfile", 65);
    shm_id = shmget(key, SHM_SIZE, 0666 | IPC_CREAT);
    shm_ptr = (int *)shmat(shm_id, NULL, 0);
    initialize_board();
    print_board();
    signal(SIGINT, handle_sigint);

    printf("Server started.\n");
    printf("Waiting for players to join...\n");

    // Wait for players to join
    int num_players = 0;
    while (num_players < 2)
    {
        int pid = fork();
        if (pid == 0)
        {
            char player_id = 'A' + num_players;
            char player_shm_id[16];
            sprintf(player_shm_id, "%d", shm_id);
            char *args[] = {"./player", &player_id, player_shm_id, NULL};
            execvp(args[0], args);
            exit(0);
        }
        else
        {
            num_players++;
        }
    }
    // Print the shared memory ID
    printf("All players joined. Game starting...\n");
    printf("Shared memory ID: %d\n\n", shm_id);

    // Randomly place ships
    int i, j;
    srand(time(NULL));
    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 5; j++)
        {
            Coordinate coord;
            do
            {
                coord.row = rand() % BOARD_SIZE;
                coord.col = rand() % BOARD_SIZE;
            } while (!is_valid_coordinate(coord) || !is_empty_coordinate(coord));
            set_ship(coord, i);
        }
    }
    print_ships();
    // Game loop
    int current_player = 0;
    while (1)
    {
        print_board();

        printf("\nPlayer %c's turn.\n", 'A' + current_player);

        // Signal the current player that it's their turn
        *shm_ptr = 'A' + current_player;

        // Wait for the current player to finish their turn
        while (*shm_ptr == 'A' + current_player)
        {
            usleep(100000); // Sleep for 100 milliseconds
        }

        // Get target coordinate from player
        Coordinate target;
        do
        {
            printf("Enter target coordinate (e.g. A3): ");
            char input[4];
            fgets(input, sizeof(input), stdin);
            target.col = input[0] - 'A';
            target.row = atoi(&input[1]);
        } while (!is_valid_coordinate(target) || !is_empty_coordinate(target));

        // Check if the target coordinate hits a ship
        int player_id = 'A' + current_player;
        if (is_ship_hit(target, player_id))
        {
            printf("Player %c: Hit!\n", player_id);
            *(shm_ptr + target.row * BOARD_SIZE + target.col) = player_id;
        }
        else
        {
            printf("Player %c: Miss!\n", player_id);
            *(shm_ptr + target.row * BOARD_SIZE + target.col) = 0;
        }

        // Print the updated board
        print_board();

        // Check if player has won
        int has_won = 1;
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
        {
            int val = *(shm_ptr + i);
            if (val == current_player + 1)
            {
                has_won = 0;
                break;
            }
        }
        if (has_won)
        {
            printf("\nPlayer %c wins!\n", 'A' + current_player);
            kill(0, SIGINT);
        }

        // Switch to the next player
        current_player = (current_player + 1) % 2;
    }

    return 0;
}