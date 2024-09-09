#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

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
    printf("    A  B  C  D  E  F  G  H  I  J \n");
    for (int i = 0; i < BOARD_SIZE; i++)
    {
        printf(" %d ", i);
        for (int j = 0; j < BOARD_SIZE; j++)
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

int is_ship_hit(Coordinate coord, int player_id)
{
    int val = *(shm_ptr + coord.row * BOARD_SIZE + coord.col);
    if (val >= 0 && val != player_id)
    {
        *(shm_ptr + coord.row * BOARD_SIZE + coord.col) = player_id;
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s player_id shm_id\n", argv[0]);
        return 1;
    }

    char player_id = argv[1][0];
    shm_id = atoi(argv[2]);

    // Attach to shared memory
    shm_ptr = (int *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (int *)-1)
    {
        perror("shmat");
        return 1;
    }

    while (1)
    {
        printf("\nPlayer %c's turn.\n", player_id);

        // Wait for the other player's turn
        while (*shm_ptr != player_id)
        {
            usleep(100000); // Sleep for 100 milliseconds
        }

        // Print the board
        print_board();

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

        // Signal the other player that it's their turn
        *shm_ptr = (player_id == 'A') ? 'B' : 'A';
    }

    // Detach from shared memory
    shmdt(shm_ptr);

    return 0;
}
