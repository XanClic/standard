#ifndef INPUT_H
#define INPUT_H

void init_mouse_input_regex(void);

int input_read(void);
void sim_input(int val);

// Note: This function takes control of "sequence".
void add_input_escape_sequence(char *sequence, int keycode);

#endif
