#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/types.h>

#define DEVICE_NAME "tictactoe"
#define CLASS_NAME "tictactoe"
#define BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Tic Tac Toe Kernel Module Game");
MODULE_VERSION("1.0");

typedef enum {
    GAME_NOT_STARTED,
    GAME_IN_PROGRESS,
    GAME_OVER
} game_state_t;

typedef enum {
    OK,
    MISSING_PIECE,
    INVALID_PIECE,
    GAME_STARTED,
    GAME_NOT_STARTED_ERR,
    INVALID_RESET,
    NOT_PLAYER_TURN,
    NOT_CPU_TURN,
    OUT_OF_BOUNDS,
    CANNOT_PLACE,
    INVALID_FORMAT,
    INVALID_BOT,
    GAME_OVER_ERR
} error_code_t;

static int major_number;
static struct class* tictactoe_class = NULL;
static struct device* tictactoe_device = NULL;
static struct cdev tictactoe_cdev;

static char game_board[3][3];
static char player_piece;
static char cpu_piece;
static game_state_t game_state;
static int player_turn;
static error_code_t last_error;
static char output_buffer[BUFFER_SIZE];

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static void initialize_board(void);
static void reset_game(void);
static int check_game_over(void);
static error_code_t process_start(const char *piece_str);
static error_code_t process_reset(const char *args);
static error_code_t process_play(const char *args);
static error_code_t process_bot(const char *args);
static void generate_board_string(void);
static void process_board(void);
static void execute_command(const char *command);

static struct file_operations fops = {
    .open = device_open,
    .read = device_read,
    .write = device_write,
    .release = device_release,
    .owner = THIS_MODULE,
};

static void initialize_board(void) {
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            game_board[i][j] = '-';
        }
    }
}

static void reset_game(void) {
    initialize_board();
    player_piece = '\0';
    cpu_piece = '\0';
    game_state = GAME_NOT_STARTED;
    player_turn = 1;
    last_error = OK;
    memset(output_buffer, 0, BUFFER_SIZE);
}

static int check_game_over(void) {
    int i, j;
    
    for (i = 0; i < 3; i++) {
        if (game_board[i][0] != '-' && 
            game_board[i][0] == game_board[i][1] && 
            game_board[i][1] == game_board[i][2]) {
            return 1;
        }
    }
    
    for (i = 0; i < 3; i++) {
        if (game_board[0][i] != '-' && 
            game_board[0][i] == game_board[1][i] && 
            game_board[1][i] == game_board[2][i]) {
            return 1;
        }
    }
    
    if (game_board[0][0] != '-' && 
        game_board[0][0] == game_board[1][1] && 
        game_board[1][1] == game_board[2][2]) {
        return 1;
    }
    
    if (game_board[0][2] != '-' && 
        game_board[0][2] == game_board[1][1] && 
        game_board[1][1] == game_board[2][0]) {
        return 1;
    }
    
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (game_board[i][j] == '-') {
                return 0;
            }
        }
    }
    
    return 2;
}

static error_code_t process_start(const char *piece_str) {
    if (game_state != GAME_NOT_STARTED) {
        return GAME_STARTED;
    }
    
    if (!piece_str || strlen(piece_str) == 0) {
        return MISSING_PIECE;
    }
    
    if (piece_str[0] != 'X' && piece_str[0] != 'O') {
        return INVALID_PIECE;
    }
    
    player_piece = piece_str[0];
    cpu_piece = (player_piece == 'X') ? 'O' : 'X';
    game_state = GAME_IN_PROGRESS;
    player_turn = 1;
    initialize_board();
    
    return OK;
}

static error_code_t process_reset(const char *args) {
    if (args && strlen(args) > 0) {
        return INVALID_RESET;
    }
    
    if (game_state == GAME_NOT_STARTED) {
        return GAME_NOT_STARTED_ERR;
    }
    
    reset_game();
    return OK;
}

static error_code_t process_play(const char *args) {
    int row, col;
    int game_result;
    
    if (game_state == GAME_NOT_STARTED) {
        return GAME_NOT_STARTED_ERR;
    }
    
    if (game_state == GAME_OVER) {
        return GAME_OVER_ERR;
    }
    
    if (!player_turn) {
        return NOT_PLAYER_TURN;
    }
    
    if (!args || strlen(args) == 0) {
        return INVALID_FORMAT;
    }
    
    if (sscanf(args, "%d,%d", &row, &col) != 2) {
        return OUT_OF_BOUNDS;
    }
    
    if (row < 1 || row > 3 || col < 1 || col > 3) {
        return OUT_OF_BOUNDS;
    }
    
    row--;
    col--;
    
    if (game_board[row][col] != '-') {
        return CANNOT_PLACE;
    }
    
    game_board[row][col] = player_piece;
    player_turn = 0;
    
    game_result = check_game_over();
    if (game_result > 0) {
        game_state = GAME_OVER;
        return GAME_OVER_ERR;
    }
    
    return OK;
}

static error_code_t process_bot(const char *args) {
    int row, col, attempts;
    unsigned int rand_row, rand_col;
    int game_result;
    
    if (game_state == GAME_NOT_STARTED) {
        return GAME_NOT_STARTED_ERR;
    }
    
    if (game_state == GAME_OVER) {
        return GAME_OVER_ERR;
    }
    
    if (args && strlen(args) > 0) {
        return INVALID_BOT;
    }
    
    if (player_turn) {
        return NOT_CPU_TURN;
    }
    
    attempts = 0;
    
    do {
        get_random_bytes(&rand_row, sizeof(rand_row));
        get_random_bytes(&rand_col, sizeof(rand_col));
        row = rand_row % 3;
        col = rand_col % 3;
        attempts++;
        
        if (attempts > 50) {
            int found = 0;
            for (row = 0; row < 3; row++) {
                for (col = 0; col < 3; col++) {
                    if (game_board[row][col] == '-') {
                        found = 1;
                        goto found;
                    }
                }
            }
            if (!found) {
                return GAME_OVER_ERR;
            }
        }
    } while (game_board[row][col] != '-');
    
found:
    game_board[row][col] = cpu_piece;
    player_turn = 1;
    
    game_result = check_game_over();
    if (game_result > 0) {
        game_state = GAME_OVER;
        return GAME_OVER_ERR;
    }
    
    return OK;
}

static void generate_board_string(void) {
    int pos = 0;
    
    pos += snprintf(output_buffer + pos, BUFFER_SIZE - pos, ". 1 2 3\n");
    pos += snprintf(output_buffer + pos, BUFFER_SIZE - pos, "1 %c %c %c\n", 
                   game_board[0][0], game_board[0][1], game_board[0][2]);
    pos += snprintf(output_buffer + pos, BUFFER_SIZE - pos, "2 %c %c %c\n", 
                   game_board[1][0], game_board[1][1], game_board[1][2]);
    pos += snprintf(output_buffer + pos, BUFFER_SIZE - pos, "3 %c %c %c\n", 
                   game_board[2][0], game_board[2][1], game_board[2][2]);
}

static void process_board(void) {
    generate_board_string();
}

static void execute_command(const char *command) {
    char cmd[32];
    char args[32] = {0};
    
    memset(output_buffer, 0, BUFFER_SIZE);
    
    if (sscanf(command, "%31s %31[^\n]", cmd, args) >= 1) {
        if (strcmp(cmd, "START") == 0) {
            last_error = process_start(args);
        } else if (strcmp(cmd, "RESET") == 0) {
            last_error = process_reset(args);
        } else if (strcmp(cmd, "PLAY") == 0) {
            last_error = process_play(args);
        } else if (strcmp(cmd, "BOT") == 0) {
            last_error = process_bot(args);
        } else if (strcmp(cmd, "BOARD") == 0) {
            process_board();
            return;
        } else {
            snprintf(output_buffer, BUFFER_SIZE, "UNKNOWN_COMMAND");
            return;
        }
    } else {
        snprintf(output_buffer, BUFFER_SIZE, "INVALID_COMMAND");
        return;
    }
    
    switch (last_error) {
        case OK: strcpy(output_buffer, "OK"); break;
        case MISSING_PIECE: strcpy(output_buffer, "MISSING_PIECE"); break;
        case INVALID_PIECE: strcpy(output_buffer, "INVALID_PIECE"); break;
        case GAME_STARTED: strcpy(output_buffer, "GAME_STARTED"); break;
        case GAME_NOT_STARTED_ERR: strcpy(output_buffer, "GAME_NOT_STARTED"); break;
        case INVALID_RESET: strcpy(output_buffer, "INVALID_RESET"); break;
        case NOT_PLAYER_TURN: strcpy(output_buffer, "NOT_PLAYER_TURN"); break;
        case NOT_CPU_TURN: strcpy(output_buffer, "NOT_CPU_TURN"); break;
        case OUT_OF_BOUNDS: strcpy(output_buffer, "OUT_OF_BOUNDS"); break;
        case CANNOT_PLACE: strcpy(output_buffer, "CANNOT_PLACE"); break;
        case INVALID_FORMAT: strcpy(output_buffer, "INVALID_FORMAT"); break;
        case INVALID_BOT: strcpy(output_buffer, "INVALID_BOT"); break;
        case GAME_OVER_ERR: strcpy(output_buffer, "GAME_OVER"); break;
        default: strcpy(output_buffer, "UNKNOWN_ERROR"); break;
    }
}

static int device_open(struct inode *inode, struct file *file) {
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    ssize_t bytes_read = 0;
    
    if (*offset > 0) {
        return 0; // EOF
    }
    
    if (copy_to_user(buffer, output_buffer, strlen(output_buffer))) {
        return -EFAULT;
    }
    
    bytes_read = strlen(output_buffer);
    *offset += bytes_read;
    
    return bytes_read;
}

static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) {
    char command[BUFFER_SIZE];
    
    if (length >= BUFFER_SIZE) {
        return -EINVAL;
    }
    
    if (copy_from_user(command, buffer, length)) {
        return -EFAULT;
    }
    
    command[length] = '\0';
    
    if (command[length - 1] == '\n') {
        command[length - 1] = '\0';
    }
    
    execute_command(command);
    
    return length;
}

static int __init tictactoe_init(void) {
    printk(KERN_INFO "TicTacToe: Initializing module\n");
    
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "TicTacToe: Failed to register device\n");
        return major_number;
    }
    
    tictactoe_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(tictactoe_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "TicTacToe: Failed to create class\n");
        return PTR_ERR(tictactoe_class);
    }
    
    tictactoe_device = device_create(tictactoe_class, NULL, 
                                    MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(tictactoe_device)) {
        class_destroy(tictactoe_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "TicTacToe: Failed to create device\n");
        return PTR_ERR(tictactoe_device);
    }
    
    cdev_init(&tictactoe_cdev, &fops);
    tictactoe_cdev.owner = THIS_MODULE;
    if (cdev_add(&tictactoe_cdev, MKDEV(major_number, 0), 1) < 0) {
        device_destroy(tictactoe_class, MKDEV(major_number, 0));
        class_destroy(tictactoe_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "TicTacToe: Failed to add cdev\n");
        return -1;
    }
    
    reset_game();
    
    printk(KERN_INFO "TicTacToe: Module initialized successfully\n");
    return 0;
}

static void __exit tictactoe_exit(void) {
    cdev_del(&tictactoe_cdev);
    
    device_destroy(tictactoe_class, MKDEV(major_number, 0));
    
    class_destroy(tictactoe_class);
    
    unregister_chrdev(major_number, DEVICE_NAME);
    
    printk(KERN_INFO "TicTacToe: Module unloaded successfully\n");
}

module_init(tictactoe_init);
module_exit(tictactoe_exit);
