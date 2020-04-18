#define USE_TI89
#define SAVE_SCREEN

#include <tigcclib.h>
#include "extgraph.h"
#include "menu.h"

#define MAX_RECURSIVE_DEPTH 1

#define UNEXPLORED 9
#define UPDATE 10
#define BOMB 11
#define FLAG 128

uint8_t unexplored[8] = {
	0b11111111,
	0b11010101,
	0b10101011,
	0b11010101,
	0b10101011,
	0b11010101,
	0b10101011,
	0b11111111
};

uint8_t border[8] = {
	0b11111111,
	0b10000001,
	0b10000001,
	0b10000001,
	0b10000001,
	0b10000001,
	0b10000001,
	0b11111111
};

uint8_t selection[8] = {
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111
};

uint8_t flag[8] = {
	0b11111111,
	0b10111111,
	0b10011111,
	0b10001111,
	0b10000111,
	0b10000011,
	0b10111111,
	0b10111111,
};

INT_HANDLER old_int5;
unsigned char lcd_buffer[LCD_SIZE];
volatile unsigned char do_quit;
volatile unsigned char did_lose;
volatile unsigned char did_win;
unsigned char *global_board;
void *kbq;
int width;
int height;
int start_x;
int start_y;
int select_x;
int select_y;
uint16_t num_bombs;
uint16_t num_flags;

char *main_menu[] = {"New Game", "Load Game", "Exit"};
char *board_sizes[] = {"20x12", "40x24", "100x60", "Custom"};
char *difficulties[] = {"Easy", "Medium", "Hard", "Custom"};
char *save_menu[] = {"Exit and save", "Exit without saving", "Cancel"};

void fill_board(unsigned char *board, int width, int height, unsigned char prob, uint16_t *bomb_count){
	int x;
	int y;

	for(x = 0; x < width; x++){
		for(y = 0; y < height; y++){
			if(rand()%256 < prob){
				board[width*y + x] = BOMB;
				if(bomb_count){
					++*bomb_count;
				}
			} else {
				board[width*y + x] = UNEXPLORED;
			}
		}
	}
}

void render_board(unsigned char *board, int width, int height, int start_x, int start_y){
	int x;
	int y;
	int px;
	int py;
	unsigned char square_value;

	for(x = start_x, px = 0; x < width && px < 153; x++, px += 8){
		for(y = start_y, py = 0; y < height && py < 93; y++, py += 8){
			square_value = board[width*y + x];
			if(square_value&FLAG){
				Sprite8_OR(px, py, 8, flag, lcd_buffer);
			} else if(square_value < UNEXPLORED){
				Sprite8_OR(px, py, 8, border, lcd_buffer);
				if(square_value){
					DrawChar(px, py, '0' + square_value, A_NORMAL);
				}
			} else if(square_value == UNEXPLORED || square_value == BOMB){
				Sprite8_OR(px, py, 8, unexplored, lcd_buffer);
			}
		}
	}
}

void render_selection(int start_x, int start_y, int select_x, int select_y){
	Sprite8_XOR((select_x - start_x)*8, (select_y - start_y)*8, 8, selection, lcd_buffer);
}

void move_up(int *start_y, int *select_y, int height, int increment){
	if(*select_y >= increment){
		*select_y -= increment;
	} else {
		*select_y = 0;
	}
	if(*start_y > *select_y){
		*start_y = *select_y;
	}
}

void move_down(int *start_y, int *select_y, int height, int increment){
	if(*select_y + increment < height){
		*select_y += increment;
	} else {
		*select_y = height - 1;
	}
	if(*start_y + 11 < *select_y){
		*start_y = *select_y - 11;
	}
}

void move_left(int *start_x, int *select_x, int width, int increment){
	if(*select_x >= increment){
		*select_x -= increment;
	} else {
		*select_x = 0;
	}
	if(*start_x > *select_x){
		*start_x = *select_x;
	}
}

void move_right(int *start_x, int *select_x, int width, int increment){
	if(*select_x + increment < width){
		*select_x += increment;
	} else {
		*select_x = width - 1;
	}
	if(*start_x + 19 < *select_x){
		*start_x = *select_x - 19;
	}
}

void reveal(unsigned char *board, int width, int height, int x, int y, int recursion){
	unsigned char num_bombs = 0;

	if(board[width*y + x] == UNEXPLORED){
		if(x){
			if(y){
				num_bombs += board[width*y + x - width - 1] == BOMB;
			}
			num_bombs += board[width*y + x - 1] == BOMB;
			if(y < height - 1){
				num_bombs += board[width*y + x + width - 1] == BOMB;
			}
		}
		if(y){
			num_bombs += board[width*y + x - width] == BOMB;
		}
		if(y < height - 1){
			num_bombs += board[width*y + x + width] == BOMB;
		}
		if(x < width - 1){
			if(y){
				num_bombs += board[width*y + x - width + 1] == BOMB;
			}
			num_bombs += board[width*y + x + 1] == BOMB;
			if(y < height - 1){
				num_bombs += board[width*y + x + width + 1] == BOMB;
			}
		}

		board[width*y + x] = num_bombs;
		if(!num_bombs && recursion){
			if(x){
				if(y){
					reveal(board, width, height, x - 1, y - 1, recursion - 1);
				}
				reveal(board, width, height, x - 1, y, recursion - 1);
				if(y < height - 1){
					reveal(board, width, height, x - 1, y + 1, recursion - 1);
				}
			}
			if(y){
				reveal(board, width, height, x, y - 1, recursion - 1);
			}
			if(y < height - 1){
				reveal(board, width, height, x, y + 1, recursion - 1);
			}
			if(x < width - 1){
				if(y){
					reveal(board, width, height, x + 1, y - 1, recursion - 1);
				}
				reveal(board, width, height, x + 1, y, recursion - 1);
				if(y < height - 1){
					reveal(board, width, height, x + 1, y + 1, recursion - 1);
				}
			}
		} else if(!num_bombs){
			if(x){
				if(y){
					board[width*y + x - width - 1] = UPDATE;
				}
				board[width*y + x - 1] = UPDATE;
				if(y < height - 1){
					board[width*y + x + width - 1] = UPDATE;
				}
			}
			if(y){
				board[width*y + x - width] = UPDATE;
			}
			if(y < height - 1){
				board[width*y + x + width] = UPDATE;
			}
			if(x < width - 1){
				if(y){
					board[width*y + x - width + 1] = UPDATE;
				}
				board[width*y + x + 1] = UPDATE;
				if(y < height - 1){
					board[width*y + x + width + 1] = UPDATE;
				}
			}
		}
	}
}

void update_board(unsigned char *board, int width, int height){
	int x;
	int y;
	unsigned char quit = 0;

	while(!quit){
		quit = 1;
		for(x = 0; x < width; x++){
			for(y = 0; y < height; y++){
				if(board[width*y + x] == UPDATE){
					quit = 0;
					board[width*y + x] = UNEXPLORED;
					reveal(board, width, height, x, y, MAX_RECURSIVE_DEPTH);
				}
			}
		}
	}
}

DEFINE_INT_HANDLER (handle_input){
	unsigned int key;
	unsigned char do_render = 0;

	ExecuteHandler(old_int5);
	while(!OSdequeue(&key, kbq)){
		if(key == KEY_UP){
			move_up(&start_y, &select_y, height, 1);
		} else if(key == KEY_DOWN){
			move_down(&start_y, &select_y, height, 1);
		} else if(key == KEY_LEFT){
			move_left(&start_x, &select_x, width, 1);
		} else if(key == KEY_RIGHT){
			move_right(&start_x, &select_x, width, 1);
		} else if(key == KEY_ESC){
			do_quit = 1;
		} else if(key == KEY_ENTER && !(global_board[width*select_y + select_x]&FLAG)){
			if(global_board[width*select_y + select_x] == BOMB){
				did_lose = 1;
			} else {
				reveal(global_board, width, height, select_x, select_y, MAX_RECURSIVE_DEPTH);
				update_board(global_board, width, height);
			}
		} else if(key == KEY_F1 && global_board[width*select_y + select_x] >= UNEXPLORED){
			if(global_board[width*select_y + select_x]&FLAG){
				num_flags--;
			} else {
				num_flags++;
			}
			global_board[width*select_y + select_x] ^= FLAG;
			if(num_flags == num_bombs){
				did_win = 1;
			}
		}
		do_render = 1;
	}
	if(do_render){
		memset(lcd_buffer, 0, LCD_SIZE);
		render_board(global_board, width, height, start_x, start_y);
		render_selection(start_x, start_y, select_x, select_y);
		memcpy(LCD_MEM, lcd_buffer, LCD_SIZE);
	}
}

int parse_integer(char *c){
	int output = 0;
	char *string_start;

	string_start = c;

	while(*c >= '0' && *c <= '9'){
		output = output*10 + *c - '0';
		c++;
	}
	if(*c){
		return -1;
	}
	return output;
}

int start_main_menu(){
	int x = 0;
	int y = 0;

	width = 20;
	height = 12;
	start_x = 0;
	start_y = 0;

	global_board = malloc(sizeof(char)*width*height);
	fill_board(global_board, width, height, 40, NULL);
	for(x = 0; x < width; x++){
		for(y = 0; y < height; y++){
			if(rand()%256 < 20){
				reveal(global_board, width, height, x, y, MAX_RECURSIVE_DEPTH);
				update_board(global_board, width, height);
			}
		}
	}
	memset(lcd_buffer, 0, LCD_SIZE);
	render_board(global_board, width, height, start_x, start_y);
	memcpy(LCD_MEM, lcd_buffer, LCD_SIZE);
	free(global_board);
	FontSetSys(F_4x6);
	FastFillRect_R(lcd_buffer, 105, 93, 159, 99, A_REVERSE);
	DrawStr(107, 94, "Ben Jones 2020", A_NORMAL);
	FontSetSys(F_6x8);
	return do_menu("Ti89 Minesweeper", main_menu, 3, lcd_buffer);
}

unsigned char save_game(char *file_name){
	FILE *fp;

	if(!file_name){
		return 1;
	}
	fp = fopen(file_name, "wb");
	if(!fp){
		return 1;
	}
	fputc((unsigned char) width, fp);
	fputc((unsigned char) height, fp);
	fputc((unsigned char) start_x, fp);
	fputc((unsigned char) start_y, fp);
	fputc((unsigned char) select_x, fp);
	fputc((unsigned char) select_y, fp);
	fwrite(&num_bombs, sizeof(uint16_t), 1, fp);
	fwrite(&num_flags, sizeof(uint16_t), 1, fp);
	fwrite(global_board, sizeof(char), width*height, fp);
	fclose(fp);

	return 0;
}

unsigned char load_game(char *file_name){
	FILE *fp;

	if(!file_name){
		return 1;
	}
	fp = fopen(file_name, "rb");
	if(!fp){
		return 1;
	}
	width = fgetc(fp);
	height = fgetc(fp);
	start_x = fgetc(fp);
	start_y = fgetc(fp);
	select_x = fgetc(fp);
	select_y = fgetc(fp);
	fread(&num_bombs, sizeof(uint16_t), 1, fp);
	fread(&num_flags, sizeof(uint16_t), 1, fp);
	global_board = malloc(sizeof(char)*width*height);
	fread(global_board, sizeof(char), width*height, fp);
	fclose(fp);

	return 0;
}

void start_game(){
	int selection;
	char *text_entry = NULL;
	char *back_message = "Back";
	char *exit_message = "Exit";

	SetIntVec(AUTO_INT_5, handle_input);
	do_quit = 0;
	did_lose = 0;
	did_win = 0;
	while(1){
		if(do_quit){
			SetIntVec(AUTO_INT_5, old_int5);
			do_quit = 0;
			selection = do_menu("Save game?", save_menu, 3, lcd_buffer);
			switch(selection){
				case 0:
					text_entry = do_text_entry("Enter save name", lcd_buffer);
					if(save_game(text_entry)){
						if(text_entry){
							free(text_entry);
							do_menu("Error saving file", &back_message, 1, lcd_buffer);
						}
						break;
					}
					free(text_entry);
					free(global_board);
					return;
				case 1:
					free(global_board);
					return;
			}
			memset(lcd_buffer, 0, LCD_SIZE);
			render_board(global_board, width, height, start_x, start_y);
			render_selection(start_x, start_y, select_x, select_y);
			memcpy(LCD_MEM, lcd_buffer, LCD_SIZE);
			SetIntVec(AUTO_INT_5, handle_input);
		}
		if(did_lose){
			SetIntVec(AUTO_INT_5, old_int5);
			do_menu("You lose!", &exit_message, 1, lcd_buffer);
			free(global_board);
			return;
		}
		if(did_win){
			SetIntVec(AUTO_INT_5, old_int5);
			do_menu("You win!", &exit_message, 1, lcd_buffer);
			free(global_board);
			return;
		}
	}
}

void do_new_game(){
	int board_size;
	int difficulty;
	int difficulty_prob = 0;
	int board_size_x = 0;
	int board_size_y = 0;
	char *bounds_message = "Must be less than 128";
	char *invalid_message = "Enter a valid integer";
	char *zero_message = "Must be positive";
	char *text_entry = NULL;

	board_size = do_menu("Enter board size", board_sizes, 4, lcd_buffer);
	switch(board_size){
		case -1:
			return;
		case 0:
			board_size_x = 20;
			board_size_y = 12;
			break;
		case 1:
			board_size_x = 40;
			board_size_y = 24;
			break;
		case 2:
			board_size_x = 100;
			board_size_y = 60;
			break;
		case 3:
			while(board_size_x <= 0){
				text_entry = do_text_entry("Enter board width", lcd_buffer);
				board_size_x = parse_integer(text_entry);
				free(text_entry);
				if(board_size_x >= 128 || board_size_x < -1){
					do_menu("Enter board width", &bounds_message, 1, lcd_buffer);
					board_size_x = -1;
				} else if(board_size_x == -1){
					do_menu("Enter board width", &invalid_message, 1, lcd_buffer);
				} else if(board_size_x == 0){
					do_menu("Enter board height", &zero_message, 1, lcd_buffer);
				}
			}
			while(board_size_y <= 0){
				text_entry = do_text_entry("Enter board height", lcd_buffer);
				board_size_y = parse_integer(text_entry);
				free(text_entry);
				if(board_size_y >= 128 || board_size_y < -1){
					do_menu("Enter board height", &bounds_message, 1, lcd_buffer);
					board_size_y = -1;
				} else if(board_size_y == -1){
					do_menu("Enter board height", &invalid_message, 1, lcd_buffer);
				} else if(board_size_y == 0){
					do_menu("Enter board height", &zero_message, 1, lcd_buffer);
				}
			}
	}
	difficulty = do_menu("Enter difficulty", difficulties, 4, lcd_buffer);
	switch(difficulty){
		case -1:
			return;
		case 0:
			difficulty_prob = 20;
			break;
		case 1:
			difficulty_prob = 40;
			break;
		case 2:
			difficulty_prob = 70;
			break;
		case 3:
			while(difficulty_prob <= 0){
				text_entry = do_text_entry("Enter difficulty (< 128)", lcd_buffer);
				difficulty_prob = parse_integer(text_entry);
				free(text_entry);
				if(difficulty_prob >= 128 || difficulty_prob < -1){
					do_menu("Enter difficulty (< 128)", &bounds_message, 1, lcd_buffer);
					difficulty_prob = -1;
				} else if(difficulty_prob == -1){
					do_menu("Enter difficulty (< 128)", &invalid_message, 1, lcd_buffer);
				} else if(difficulty_prob == 0){
					do_menu("Enter difficulty (< 128)", &zero_message, 1, lcd_buffer);
				}
			}
	}

	global_board = malloc(sizeof(char)*board_size_x*board_size_y);
	num_bombs = 0;
	fill_board(global_board, board_size_x, board_size_y, difficulty_prob, &num_bombs);
	do_quit = 0;
	select_x = 0;
	select_y = 0;
	start_x = 0;
	start_y = 0;
	memset(lcd_buffer, 0, LCD_SIZE);
	render_board(global_board, width, height, start_x, start_y);
	render_selection(start_x, start_y, select_x, select_y);
	memcpy(LCD_MEM, lcd_buffer, LCD_SIZE);
	width = board_size_x;
	height = board_size_y;
	start_game();
}

void do_load_game(){
	char *text_entry;
	char *error_message = "Error loading file";

	text_entry = do_text_entry("Enter save name", lcd_buffer);
	if(load_game(text_entry)){
		if(text_entry){
			free(text_entry);
			do_menu("Enter save name", &error_message, 1, lcd_buffer);
		}
		return;
	}
	free(text_entry);
	start_game();
}

void _main(){
	int selection;

	do_quit = 0;
	global_board = NULL;

	kbq = kbd_queue();
	PortSet(lcd_buffer, 239, 127);
	randomize();
	old_int5 = GetIntVec(AUTO_INT_5);
	FontSetSys(F_6x8);
	while((selection = start_main_menu()) != 2){//While the user doesn't choose to exit
		if(selection == 0){
			do_new_game();
		} else if(selection == 1){
			do_load_game();
		} else if(selection == 2){
			break;
		}
	}
	SetIntVec(AUTO_INT_5, old_int5);
	PortRestore();
}

