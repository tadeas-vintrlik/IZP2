/**
 * @file sps.c
 * @brief More advanced spreadsheet editor
 * @author Tadeas Vintrlik (xvintr04)
 * @date - start November 19 2020
 * @date - last edit December 5 2020
 *
 *   ___
 * │\. ./│________/
 *   \o/\   __   │
 *       \_│  │_/
 *
 * Asciipes Fik for good luck
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Constants
#define CHUNK 128	// Size to use for cell by default
#define ALLOC_FAILED 2
#define SUCCESS 1
#define EOL -2
#define NO_MODS 6	// Length of mod_list array
#define NO_DATA 7	// Length of data_list array
#define TWO_ARG_DATA 2 // Index in data_list where commands with [R,C] begin
#define NO_VAR 4
#define MAX_VAR 10	// Variables _0 to _9 + _ for coordinates
#define SLASH -1	// if under-slash was contained in a BOX selection
#define NO_TYPE_FUNCTIONS 4	// Amount of is_type functions
#define VAR_LEN_INDEX 2 // Len of variable identifier one number 0-9
#define VAR_LEN_NAME 6 // Len of variable identifier "def _", etc.
#define FIND_LEN 5 // Minimal length of [find .*] selection
#define SET_LEN 5 // Minimal length of set .*

// Structures
typedef struct
{
	int length; // Length is the length of the actual content
	int size; // Size is the currently allocated size of content
	char *content;
} col_t;

typedef struct
{
	int no_cols;
	col_t *cols;
} row_t;

typedef struct
{
	int no_rows;
	row_t *rows;
} table_t;

// What type of selection CELL is for [R,C], ROW if for [R,_] and so on
typedef enum { CELL, ROW, COL, BOX, TABLE, MIN, MAX, STR, TMP_VAR,
	INVALID_S } stype_t;
const char TEMP_stype_list[][10] = { "CELL", "ROW", "COL", "BOX", "TABLE",
	"MIN", "MAX", "STR", "TMP_VAR", "INVALID_S" };

// What type of modification command was called
typedef enum { IROW, AROW, DROW, ICOL, ACOL, DCOL } mtype_t;
const char mod_list[NO_MODS][5] = { "irow", "arow", "drow", "icol", "acol", "dcol"};

// What type of data command was called
// white-spaces from swap forward are intentional, used for string comparison
typedef enum { SET, CLEAR, SWAP, SUM, AVG, COUNT, LEN } dtype_t;
const char data_list[NO_DATA][7] = { "set", "clear", "swap ", "sum ", "avg ", "count ",
	"len "};

// What type of variable command was called
typedef enum { DEF, USE, INC, SET_VAR } vtype_t;
const char var_list[NO_VAR][6] = { "def _", "use _", "inc _", "[set]" };

typedef enum { VARIABLE, MODIFICATION, DATA, SELECTION, INVALID} cmd_types_t;
const cmd_types_t cmd_types_s[] = { VARIABLE, MODIFICATION, DATA, SELECTION,
	INVALID };
const char TEMP_cmd_type_list[][13] = { "VARIABLE", "MODIFICATION", "DATA",
	"SELECTION", "CONTROL", "INVALID" };

// Structure containing current selection of rows and columns
typedef struct
{
	stype_t type;
	int row1;
	int col1;
	int row2;
	int col2;
	char *str;
} selection_t;

typedef struct
{
	cmd_types_t type;
	char cmd_name[15];
	int cmd_num;
	int arg1;
	int arg2;
	char *str;
	selection_t *selection;
} command_t;

typedef struct
{
	int size_c;	// size of commands array
	int count_c;	// amount of commands stored in array
	int size_s;	// size of selections array
	int count_s;	// amount of selections stored in array
	command_t *commands;
	selection_t *selections;
	char *delim;
} call_t;

typedef struct
{
	char *values[MAX_VAR];
	int lengths[MAX_VAR];
	int sizes[MAX_VAR];
	selection_t *selection;
} variables_t;

// Constructors and destructors
void cell_dtor(col_t *col)
{
	free(col->content);
	col->content = NULL;
}

void row_dtor(row_t *row)
{
	if (row->cols == NULL)
		return;
	for(int i = 0; i < row->no_cols; i++)
		cell_dtor(&row->cols[i]);
	free(row->cols);
	row->cols = NULL;
}

void table_dtor(table_t *table)
{
	for (int i = 0; i < table->no_rows; i++)
		row_dtor(&table->rows[i]);
	free(table->rows);
	table->rows = NULL;
	table->no_rows = 0;
}

void command_dtor(command_t *cmd)
{
	free(cmd->str);
	cmd->str = NULL;
}

void selection_dtor(selection_t *sel)
{
	free(sel->str);
	sel->str = NULL;
}

void call_dtor(call_t *call)
{
	int len = call->count_s;
	for (int i = 0; i < len; i++)
		selection_dtor(&call->selections[i]);
	free(call->selections);
	call->selections = NULL;
	len = call->count_c;
	for (int i = 0; i < len; i++)
		command_dtor(&call->commands[i]);
	free(call->commands);
	call->commands = NULL;
}

void variables_dtor(variables_t *variable)
{
	for (int i = 0; i < MAX_VAR; i++)
		free(variable->values[i]);
	if (variable->selection != NULL)
		selection_dtor(variable->selection);
}

void error_msg(void)
{
	fprintf(stderr, "Allocation failed!\n\
The programme likely ran out of memory.\n\
Terminating now!\n");
}

void alloc_fail_table_call(table_t *t, call_t *c)
{
	error_msg();
	table_dtor(t);
	call_dtor(c);
	exit(EXIT_FAILURE);
}

void alloc_fail(table_t *t, FILE *file)
{
	error_msg();
	table_dtor(t);
	fclose(file);
	exit(EXIT_FAILURE);
}

void alloc_fail_table(table_t *t)
{
	error_msg();
	table_dtor(t);
	exit(EXIT_FAILURE);
}

void alloc_fail_call(call_t *c)
{
	error_msg();
	call_dtor(c);
	exit(EXIT_FAILURE);

}

void alloc_fail_nothing()
{
	error_msg();
	exit(EXIT_FAILURE);
}

col_t col_ctor(void)
{
	col_t new_col = {.length = 0, .size = 0, .content = NULL };
	return new_col;

}

row_t row_ctor(int no_cols)
{
	row_t new_row = { .no_cols=no_cols, .cols=NULL };
	return new_row;
}

void row_alloc(row_t *row, table_t *table, FILE *file)
{
	col_t *col_ptr = malloc(row->no_cols * sizeof(col_t));
	if (col_ptr == NULL)
		alloc_fail(table, file);
	row->cols = col_ptr;
}

void col_alloc(col_t *col, table_t *table, FILE *file)
{
	col->size = CHUNK;
	col->content = realloc(col->content, col->size);
	if (col->content == NULL)
		alloc_fail(table, file);
	strcpy(col->content, "");
}

/**
 * Set content for according cell of the table
 * @param table_t *table - table in which to make the change
 * @param int row - index of row where to set it
 * @param int col - index of column where to set it
 * @param char *value - new value by which to replace content
 */
void set_cell_value(table_t *table, int row, int col, char *value, void *freeptr)
{
	int len_new = strlen(value);
	int *size;
	size = &table->rows[row].cols[col].size;
	char **content;
	content = &table->rows[row].cols[col].content;
	// if content buffer is too small double the size, +1 for '\0'
	while (*size < len_new + 1)
	{
		*size = *size * 2;
		*content = realloc(*content, *size);
		if (content == NULL)
		{
			free(freeptr);
			alloc_fail_table(table);
		}
	}
	strcpy(*content, value);
	table->rows[row].cols[col].length = len_new;
	// If the new cell content would be significantly smaller, shrink it
	while (*size / 2 > len_new + 1 && *size > CHUNK)
	{
		*size = *size / 2;
		// Can it even fail? Well better be sure
		*content = realloc(*content, *size);
		if (content == NULL)
		{
			free(freeptr);
			alloc_fail_table(table);
		}
	}
}

// Same as set_cell_value but called while file is still open, thus if
// allocation fails it will close it
void fill_cell_value(FILE *file, table_t *table, int row, int col, char *value)
{
	int len_new = strlen(value);
	int *size;
	size = &table->rows[row].cols[col].size;
	char **content;
	content = &table->rows[row].cols[col].content;
	// if content buffer is too small double the size, +1 for '\0'
	while (*size < len_new + 1)
	{
		*size = *size * 2;
		*content = realloc(*content, *size);
		if (content == NULL)
			alloc_fail(table, file);
	}
	strcpy(*content, value);
	table->rows[row].cols[col].length = len_new;
}

 table_t table_ctor(int no_rows, int no_cols, FILE *file)
{
	// Create table itself
	row_t *row_ptr = malloc(no_rows * sizeof(row_t));
	table_t table = {no_rows, row_ptr};
	if (row_ptr == NULL)
		alloc_fail(&table, file);

	// Create rows
	for (int i = 0; i < no_rows; i++)
		table.rows[i] = row_ctor(no_cols);

	// Allocate rows
	for (int i = 0; i < no_rows; i++)
	{
		row_alloc(&table.rows[i], &table, file);

		// Create columns
		for (int j = 0; j < no_cols; j++)
			table.rows[i].cols[j] = col_ctor();
	}

	// Allocate cells
	for (int i = 0; i < no_rows; i++)
		for (int j = 0; j < no_cols; j++)
			col_alloc(&table.rows[i].cols[j], &table, file);

	return table;
}

// Pass table in case it fails and we must deallocate
call_t call_ctor(void)
{
	call_t new;
	new.size_c = new.size_s = CHUNK;
	new.count_c = new.count_s = 0;
	new.commands = malloc(new.size_c * sizeof(command_t));
	if (new.commands == NULL)
		alloc_fail_nothing();
	new.selections = malloc(new.size_s * sizeof(selection_t));
	if (new.selections == NULL)
		alloc_fail_nothing();
	return new;
}

void call_add_cmd(call_t *call, command_t new)
{
	if (call->size_c == call->count_c)
	{
		call->size_c = call->size_c * 2;
		call->commands = realloc(call->commands, call->size_c * sizeof(command_t));
		if (call->commands == NULL)
		{
			call_dtor(call);
			alloc_fail_nothing();
		}
	}
	call->commands[call->count_c++] = new;
}

void call_add_selection(call_t *call, selection_t new)
{
	if (call->size_s == call->count_s)
	{
		call->size_s = call->size_s * 2;
		call->selections = realloc(call->commands, call->size_s * sizeof(selection_t));
		if (call->selections == NULL)
		{
			call_dtor(call);
			alloc_fail_nothing();
		}
	}
	call->selections[call->count_s++] = new;
}

variables_t variables_ctor(call_t *call)
{
	variables_t new;
	for (int i = 0; i < MAX_VAR; i++)
		new.values[i] = NULL;

	for (int i = 0; i < MAX_VAR; i++)
	{
		new.sizes[i] = CHUNK;
		new.lengths[i] = 0;
	}

	for (int i = 0; i < MAX_VAR; i++)
	{
		new.values[i] = calloc(new.sizes[i], sizeof(char));
		if (new.values[i] == NULL)
		{
			variables_dtor(&new);
			error_msg();
		}
	}
	new.selection = &call->selections[0];
	return new;
}

void variable_store(table_t *table, variables_t *variables, int index, char *value)
{
	int len_new = strlen(value);
	int *size;
	size = &variables->sizes[index];
	char **content;
	content = &variables->values[index];
	// if content buffer is too small double the size, +1 for '\0'
	while (*size < len_new + 1)
	{
		*size = *size * 2;
		*content = realloc(*content, *size);
		if (content == NULL)
		{
			table_dtor(table);
			variables_dtor(variables);
			error_msg();
		}
	}
	strcpy(*content, value);
	variables->lengths[index] = len_new;
	// If the new cell content would be significantly smaller, shrink it
	while (*size / 2 > len_new + 1 && *size > CHUNK)
	{
		*size = *size / 2;
		// Can it even fail? Well better be sure
		*content = realloc(*content, *size);
		if (content == NULL)
		{
			table_dtor(table);
			variables_dtor(variables);
			error_msg();
		}
	}
}


void row_swap(row_t *a, row_t *b)
{
	row_t temp = *a;
	*a = *b;
	*b = temp;
}

void col_swap(col_t *a, col_t *b)
{
	col_t temp = *a;
	*a = *b;
	*b = temp;
}

/**
 * Append multiple rows to the end of the table, for performance reasons
 */
void table_add_rows(table_t *table, int count)
{
	int first_uninit = table->no_rows; // index of first row not initialised
	table->no_rows += count;
	row_t *new_ptr = realloc(table->rows, table->no_rows * sizeof(row_t));
	if (new_ptr == NULL)
		alloc_fail_table(table);
	table->rows = new_ptr;
	for (int i = first_uninit; i < table->no_rows; i++)
	{
		table->rows[i].no_cols = table->rows[1].no_cols;
		row_alloc(&table->rows[i], table, NULL);
		for (int j = 0; j < table->rows[i].no_cols; j++)
			table->rows[i].cols[j] = col_ctor();
		for (int j = 0; j < table->rows[i].no_cols; j++)
			col_alloc(&table->rows[i].cols[j], table, NULL);
	}
}

/**
 * removes last row from table
 */
void table_delete_row(table_t *table)
{
	int last = --table->no_rows;
	row_dtor(&table->rows[last]);
	row_t *new_ptr = realloc(table->rows, table->no_rows * sizeof(row_t));
	if (new_ptr == NULL && table->no_rows != 0)
		alloc_fail_table(table);
	table->rows = new_ptr;
}

void row_add_cols(row_t *row, table_t *table, int count)
{
	int first_uninit = row->no_cols;
	row->no_cols += count;
	col_t *new_ptr = realloc(row->cols, row->no_cols * sizeof(col_t));
	if (new_ptr == NULL)
		alloc_fail_table(table);
	row->cols = new_ptr;
	for (int i = first_uninit; i < row->no_cols; i++)
	{
		row->cols[i] = col_ctor();
		col_alloc(&row->cols[i], table, NULL);
	}
}

/**
 * Append count column to end of each row, for the sake of performance
 */
void table_add_cols(table_t *table, int count)
{
	for (int i = 0; i < table->no_rows; i++)
		row_add_cols(&table->rows[i], table, count);
}

void row_delete_col(row_t *row, table_t *table)
{
	int last = --row->no_cols;
	free(row->cols[last].content);
	col_t *new_ptr = realloc(row->cols, row->no_cols * sizeof(col_t));
	if (new_ptr == NULL)
		alloc_fail_table(table);
	row->cols = new_ptr;
}

/**
 * removes last column from each row
 */
void table_delete_col(table_t *table)
{
	for (int i = 0; i < table->no_rows; i++)
		row_delete_col(&table->rows[i], table);
}

/**
 * Get content of matching cell
 * table_t table - table where to find the cell
 * int row - index of row
 * int col - index of col
 * return char* - the content string
 */
char *get_cell_content(const table_t *table, int row, int col)
{
	return table->rows[row].cols[col].content;
}

// same as  get_cell_content but stores numeric value into *var
// return false if it is not numeric cell
bool get_cell_numeric(const table_t *table, int row, int col, double *var)
{
	char *content = get_cell_content(table, row, col);
	char *endptr = NULL;
	*var = strtod(content, &endptr);
	if (strlen(content) == 0)
		return false;
	int length = strlen(endptr);
	for (int i = 0; i < length; i++)
		if (endptr[i] != ' ')
			return false;

	if (length == 0)
		return true;
	else
		return false;
}

// Find the cell with minimal value from table and old store it into new
bool find_min_cell(const table_t *table, selection_t old, selection_t *new)
{
	double num = 0;
	bool no_min = true;
	double min;
	new->type = CELL;
	switch (old.type)
	{
		case CELL:
			if (get_cell_numeric(table, old.row1 - 1, old.col1 - 1, &num))
			{
					new->row1 = old.row1;
					new->col1 = old.col1;
					return true;
			}
			else
				return false;
			break;
		case ROW:
			for (int i = 0; i < table->rows[0].no_cols; i++)
			{
				if (get_cell_numeric(table, old.row1 - 1, i, &num))
				{
					if (no_min || num < min)
					{
						min = num;
						no_min = false;
						new->row1 = old.row1;
						new->col1 = i + 1; // since selections start at 1
					}
				}
			}
			if (new->col1 == 0)
				return false;
			return true;
			break;
		case COL:
			for (int i = 0; i < table->no_rows; i++)
			{
				if (get_cell_numeric(table, i, old.col1 - 1, &num))
				{
					if (no_min || num < min)
					{
						min = num;
						no_min = false;
						new->row1 = i + 1; // since selections start at 1
						new->col1 = old.col1;
					}
				}
			}
			if (new->row1 == 0)
				return false;
			return true;
			break;
		case BOX:
			if (old.row2 == SLASH)
				old.row2 = table->no_rows;
			if (old.col2 == SLASH)
				old.col2 = table->rows[0].no_cols;
			for (int i = old.row1 - 1; i < old.row2; i++)
			{
				for (int j = old.col1 - 1; j < old.col2; j++)
				{
					if (get_cell_numeric(table, i, j, &num))
					{
						if (no_min || num < min)
						{
							min = num;
							no_min = false;
							new->row1 = i + 1; // since selections start at 1
							new->col1 = j + 1; // since selections start at 1
						}
					}
				}
			}
			if (new->row1 == 0 || new->col1 == 0)
				return false;
			return true;
			break;
		case TABLE:
			for (int i = 0; i < table->no_rows; i++)
			{
				for (int j = 0; j < table->rows[0].no_cols; j++)
				{
					if (get_cell_numeric(table, i, j, &num))
					{
						if (no_min || num < min)
						{
							min = num;
							no_min = false;
							new->row1 = i + 1; // since selections start at 1
							new->col1 = j + 1; // since selections start at 1
						}
					}
				}
			}
			if (new->row1 == 0 || new->col1 == 0)
				return false;
			return true;
			break;
		case MIN:
			// This will not occur since it will be replaced by CELL
			break;
		case MAX:
			// This will not occur since it will be replaced by CELL
			break;
		case STR:
			// This will not occur since it will be replaced by CELL
			break;
		case TMP_VAR:
			// This will not occur since it will be replaced by CELL
			break;
		case INVALID_S:
			// This will never occur
			break;
	}
	return false;
}

// Find the cell with minimal value from table and old store it into new
// basically same as find_min_cell, it could somehow be one function, no time
bool find_max_cell(const table_t *table, selection_t old, selection_t *new)
{
	double num = 0;
	bool no_max = true;
	double max;
	new->type = CELL;
	switch (old.type)
	{
		case CELL:
			if (get_cell_numeric(table, old.row1 - 1, old.col1 - 1, &num))
			{
					new->row1 = old.row1;
					new->col1 = old.col1;
					return true;
			}
			else
				return false;
			break;
		case ROW:
			for (int i = 0; i < table->rows[0].no_cols; i++)
			{
				if (get_cell_numeric(table, old.row1 - 1, i, &num))
				{
					if (no_max || num > max)
					{
						max = num;
						no_max = false;
						new->row1 = old.row1;
						new->col1 = i + 1; // since selections start at 1
					}
				}
			}
			if (new->col1 == 0)
				return false;
			return true;
			break;
		case COL:
			for (int i = 0; i < table->no_rows; i++)
			{
				if (get_cell_numeric(table, i, old.col1 - 1, &num))
				{
					if (no_max || num > max)
					{
						max = num;
						no_max = false;
						new->row1 = i + 1; // since selections start at 1
						new->col1 = old.col1;
					}
				}
			}
			if (new->row1 == 0)
				return false;
			return true;
			break;
		case BOX:
			if (old.row2 == SLASH)
				old.row2 = table->no_rows;
			if (old.col2 == SLASH)
				old.col2 = table->rows[0].no_cols;
			for (int i = old.row1 - 1; i < old.row2; i++)
			{
				for (int j = old.col1 - 1; j < old.col2; j++)
				{
					if (get_cell_numeric(table, i, j, &num))
					{
						if (no_max || num > max)
						{
							max = num;
							no_max = false;
							new->row1 = i + 1; // since selections start at 1
							new->col1 = j + 1; // since selections start at 1
						}
					}
				}
			}
			if (new->row1 == 0 || new->col1 == 0)
				return false;
			return true;
			break;
		case TABLE:
			for (int i = 0; i < table->no_rows; i++)
			{
				for (int j = 0; j < table->rows[0].no_cols; j++)
				{
					if (get_cell_numeric(table, i, j, &num))
					{
						if (no_max || num > max)
						{
							max = num;
							no_max = false;
							new->row1 = i + 1; // since selections start at 1
							new->col1 = j + 1; // since selections start at 1
						}
					}
				}
			}
			if (new->row1 == 0 || new->col1 == 0)
				return false;
			return true;
			break;
		case MIN:
			// This will not occur since it will be replaced by CELL
			break;
		case MAX:
			// This will not occur since it will be replaced by CELL
			break;
		case STR:
			// This will not occur since it will be replaced by CELL
			break;
		case TMP_VAR:
			// This will not occur since it will be replaced by CELL
			break;
		case INVALID_S:
			// This will never occur
			break;
	}
	return false;
}

/**
 * Return if c is substring of delim
 * @param char c - character which to check
 * @param char *delim - string which to check against
 * @return boolean true if is a delim character
 */
bool is_delim(char c, char *delim)
{
	while (*delim)
		if (c == *delim++)
			return true;
	return false;
}

void unescape_string(char *string, table_t *table, call_t *call)
{
	int length = strlen(string);
	char *new = calloc(length + 1, sizeof(char));
	if (new == NULL)
		alloc_fail_table_call(table, call);
	int quote_open = false;
	bool escaped = false;
	char *iterator = new;
	for (int i = 0; i < length; i++)
	{
		char c = string[i];
		// Skip backslashes
		if (c == '\\' && !escaped)
		{
				escaped = true;
				continue;
		}
		// Handle quoting
		if (c == '\"' && !escaped)
		{
			if (!quote_open)
				quote_open = true;
			else
				quote_open = false;
			continue;
		}
		// End of cell
		if (!quote_open && (c == '\n' || (is_delim(c, call->delim)
						&& !escaped)))
			break;

		*iterator++ = c;
		escaped = false;
	}
	*iterator++ = '\0';
	strcpy(string, new);
	free(new);
}

bool col_substr(col_t col, char *value)
{
	int length = strlen(value);
	if (length > (int) strlen(col.content))
		return false;

	int match = 0;
	for (int i = 0; col.content[i] != '\0'; i++)
	{
		if (match == length)
			return true;
		if (value[match] == col.content[i])
			match++;
		else
			match = 0;
	}
	return match == length;
}

bool find_substr_cell(table_t *table, selection_t old, selection_t current,
		selection_t *new, call_t *call)
{
	int row1 = old.row1 - 1;
	int col1 = old.col1 - 1;
	char *str = current.str;
	unescape_string(str, table, call);
	new->type = CELL;
	switch (old.type)
	{
		case CELL:
			if (col_substr(table->rows[row1].cols[col1], str))
			{
					new->row1 = old.row1;
					new->col1 = old.col1;
					return true;
			}
			else
				return false;
			break;
		case ROW:
			for (int i = 0; i < table->rows[0].no_cols; i++)
			{
				if (col_substr(table->rows[row1].cols[i], str))
				{
					new->row1 = old.row1;
					new->col1 = i +  1; // since selections start at 1
				}
			}
			if (new->col1 == 0)
				return false;
			return true;
			break;
		case COL:
			for (int i = 0; i < table->no_rows; i++)
			{
				if (col_substr(table->rows[i].cols[col1], str))
				{
					new->row1 = i + 1; // since selections start at 1
					new->col1 = old.col1;
				}
			}
			if (new->row1 == 0)
				return false;
			return true;
			break;
		case BOX:
			if (old.row2 == SLASH)
				old.row2 = table->no_rows;
			if (old.col2 == SLASH)
				old.col2 = table->rows[0].no_cols;
			for (int i = old.row1 - 1; i < old.row2; i++)
			{
				for (int j = old.col1 - 1; j < old.col2; j++)
				{
					if (col_substr(table->rows[i].cols[j], str))
					{
						new->row1 = i + 1; // since selections start at 1
						new->col1 = j + 1; // since selections start at 1
					}
				}
			}
			if (new->row1 == 0 || new->col1 == 0)
				return false;
			return true;
			break;
		case TABLE:
			for (int i = 0; i < table->no_rows; i++)
			{
				for (int j = 0; j < table->rows[0].no_cols; j++)
				{
					if (col_substr(table->rows[i].cols[j], str))
					{
						new->row1 = i + 1; // since selections start at 1
						new->col1 = j + 1; // since selections start at 1
					}
				}
			}
			if (new->row1 == 0 || new->col1 == 0)
				return false;
			return true;
			break;
		case MIN:
			// This will not occur since it will be replaced by CELL
			break;
		case MAX:
			// This will not occur since it will be replaced by CELL
			break;
		case STR:
			// This will not occur since it will be replaced by CELL
			break;
		case TMP_VAR:
			// This will not occur since it will be replaced by CELL
			break;
		case INVALID_S:
			// This will never occur
			break;
	}
	return true;
}

void print_cell(FILE *file, char *content, char *delim)
{
	int length = strlen(content);
	bool contains_delim = false;
	for (int i = 0; i < length; i++)
		if (is_delim(content[i], delim))
			contains_delim = true;

	if (contains_delim)
		putc('\"', file);
	for (int i = 0; i < length; i++)
	{
		char cur = content[i];
		if (cur == '\\' || cur == '\"')
		{
			putc('\\', file);
			putc(cur, file);
		}
		else
			putc(cur, file);
	}
	if (contains_delim)
		putc('\"', file);
}

/**
 * Print given table to standard output
 * @param table_t table - table which to print
 * @param char *delim - what to use as a delimiter
 */
void write_table(FILE *file, table_t table, char *delim)
{
	for (int i = 0; i < table.no_rows; i++)
	{
		for (int j = 0; j < table.rows[i].no_cols; j++)
		{
			print_cell(file, get_cell_content(&table, i, j), delim);
			// if not last column also print delimiter
			if (j != table.rows[i].no_cols - 1)
				fprintf(file, "%c", delim[0]);
		}
		fprintf(file, "\n");
	}
}

/**
 * Delimiter can be invalid if it contains escape sequence characters
 * check if delimiter is valid
 * @param char *delim - delimiter to check
 * @return boolean - true if valid delimiter
 */
bool valid_delim(char *delim)
{
	while (*delim)
	{
		if (*delim == '\\' || *delim == '\"')
			return false;
		else
			delim++;
	}
	return true;
}

/**
 * Return number of rows, load number of columns into the cols variable
 * @param FILE *file - file which to read
 * @param char *delim - what to use as delimiter
 * @param int *cols - where to store the number of columns
 * @return int - number of rows
 */
int get_sizes(FILE *file, char *delim, int *cols)
{
	char c;
	int no_rows = 0;
	int cols_most = 0, cols_current = 0;
	bool quote_open = false;
	while ((c = fgetc(file)) != EOF)
	{
		if (c == '\"')
		{
			if (!quote_open)
				quote_open = true;
			else
				quote_open = false;
		}
		// Only count cols on first row
		if (!quote_open && (is_delim(c, delim) || c == '\n'))
			cols_current++;
		if (c == '\n')
		{
			no_rows++;
			if (cols_current > cols_most)
				cols_most = cols_current;
			cols_current = 0;
		}
	}
	rewind(file);
	*cols = cols_most;
	return no_rows;
}

/**
 * Get what would be one cell in the table from the file
 * @param FILE *file - file to read
 * @param char *delim - what to use as delimiter
 * @param int *size - size of the content buffer, can be resized since maximum
 * length of cell is not specified
 * @param char *content - string into which to store the read cell from file
 * @return int - SUCCESS if everything went OK, EOL if it was last cell of row
 */
int read_one_cell(table_t *table, FILE *file, char *delim, int *size,
		char *content)
{
	int c;
	bool quote_open = false;
	bool escaped = false;
	int chars_found = 1; // start with one for '\0'
	while ((c = fgetc(file)) != EOF)
	{
		chars_found++;
		if (chars_found >= *size)
		{
			*size = *size * 2;
			content = realloc(content, *size * sizeof(char));
			if (content == NULL)
				alloc_fail(table, file);
		}
		// Skip backslashes
		if (c == '\\')
		{
			escaped = true;
			continue;
		}
		// Handle quoting
		if (c == '\"' && !escaped)
		{
			if (!quote_open)
				quote_open = true;
			else
				quote_open = false;
			continue;
		}
		// End of cell
		if (!quote_open && (c == '\n' || (is_delim(c, delim) && !escaped)))
			break;

		*content++ = c;
		escaped = false;
	}
	*content++ = '\0';
	if (quote_open)
	{
		fprintf(stderr,
				"Unexpected input! Unbalanced quotes.\nTerminating...\n");
		table_dtor(table);
		fclose(file);
		exit(EXIT_FAILURE);
	}
	if (c == '\n')
		return EOL;
	else
		return SUCCESS;
}

/**
 * Fill table cell by cell
 * @param table_t *table - where to fill found values
 * @param File *file - where to get the values
 * @param char *delim - what to use as delimiter
 * @return int - SUCCESS if everything went OK
 */
void fill_table_with_data(table_t *table, FILE *file, char *delim)
{
	int rows = table->no_rows;
	int cols = table->rows[0].no_cols;
	for (int i = 0; i < rows; i++)
	{
		int eol_found = 0; // If newline was already seen
		for (int j = 0; j < cols; j++)
		{
			int *size = &(table->rows[i].cols[j].size);
			char *content = malloc(CHUNK * sizeof(char));
			if (content == NULL)
				alloc_fail(table, file);
			if (!eol_found)
				if(read_one_cell(table, file, delim, size, content) == EOL)
					eol_found = 1;
			// First cell where newline was found will still have content
			if (eol_found == 2)
				strcpy(content, "");
			if (eol_found == 1)
				eol_found ++;
			fill_cell_value(file, table, i, j, content);
			free(content);
		}
	}
}

int get_no_commas(const char *str)
{
	int no_commas = 0;
	int length = strlen(str);

	for (int i = 0; i < length; i++)
		if (str[i] == ',')
			no_commas++;

	return no_commas;
}

/**
 * Selection should look something should be in []
 * 1, 3 or no commas are inside
 */
bool is_selection(char *cmd)
{
	int length = strlen(cmd);
	int no_commas = get_no_commas(cmd);

	if (no_commas == 0 || no_commas == 1 || no_commas == 3)
		return cmd[0] == '[' && cmd[length - 1] == ']';
	return false;
}


bool is_modification(char *cmd)
{
	for (int i = 0; i < NO_MODS; i++)
		if (strcmp(cmd, mod_list[i]) == 0)
				return true;
	return false;
}

// Return if haystack begins with needle
bool begins_with(const char* haystack, const char* needle)
{
	int hs_len = strlen(haystack);
	int n_len = strlen(needle);
	if (hs_len < n_len)
		return false;

	for (int i = 0; i < n_len; i++)
		if (haystack[i] != needle[i])
			return false;

	return true;
}

bool single_word(char *str, int text_start)
{
	int length = strlen(str);
	bool quoted = false;
	bool escaped = false;
	for (int i = text_start; i < length; i++)
	{
		if (str[i] == '\\' && !escaped)
		{
				escaped = true;
				continue;
		}
		if (str[i] == '\"' && !escaped)
		{
			if (!quoted)
				quoted = true;
			else
				quoted = false;
		}

		if (str[i] == ' ' && !quoted)
			return false;

		escaped = false;
	}
	if (quoted)
		return false; // if quote still open
	return true;
}

// Checks if cmd is in format "set STR", where STR is one word (+escaped)
bool is_set_str(char *cmd)
{
	int length = strlen(cmd);
	// if shorter than SET_LEN it can't possibly be set with an argument
	if (length < SET_LEN)
		return false;

	if (!begins_with(cmd, "set "))
			return false;

	if(!single_word(cmd, SET_LEN - 1))
		return false;

	return true;
}

// Check if in format "name [R,C]", where name is from data_list
bool is_two_arg_data(char *cmd)
{
	int cmd_match = -1; // index of matching name in data_list

	for (int i = TWO_ARG_DATA; i < NO_DATA; i++)
		if (begins_with(cmd, data_list[i]))
				cmd_match = i;

	if (cmd_match == -1)
		return false;

	// Name was OK, check the [R,C] argument
	int start = strlen(data_list[cmd_match]); // How many characters is "name "
	int no_commas = get_no_commas(cmd);
	int length = strlen(cmd);
	return no_commas == 1 && cmd[start] == '[' && cmd[length - 1] == ']';
}

bool is_data(char *cmd)
{
	// option 1: is "set STR", where STR is \w
	if (is_set_str(cmd))
		return true;
	// option 2: is "clear"
	if (strcmp(cmd, "clear") == 0)
		return true;
	// option 3: is "cmd [R,C]", where cmd is some other command in data_list
	if (is_two_arg_data(cmd))
		return true;
	// data_list
	return false;
}

bool is_var(char *cmd)
{
	// check for "function _[0-9]", last one is in format [set]
	for (int i = 0; i < NO_VAR - 1; i++)
	{
		for (int j = 0; j < MAX_VAR; j++)
		{
			char number[VAR_LEN_INDEX];
			sprintf(number, "%d", j);
			char current_var[VAR_LEN_NAME];
			strcpy(current_var, var_list[i]);
			strcat(current_var, number);
			if (strcmp(current_var, cmd) == 0)
				return true;
		}
	}

	// Check for [set]
	if (strcmp("[set]", cmd) == 0)
		return true;

	return false;
}


/**
 * @param char *cmd - string of command
 */
cmd_types_t get_command_type(char *cmd)
{
	bool (*func_ptr[]) (char*) = { is_var, is_modification, is_data, is_selection };
	for (int i = 0; i < NO_TYPE_FUNCTIONS; i++)
		if ((*func_ptr[i]) (cmd))
			return cmd_types_s[i];
	return INVALID;
}

void invalid_selection(char *cmd_name, call_t *call)
{
	fprintf(stderr, "Invalid selection for %s!\n", cmd_name);
	call_dtor(call);
	exit(EXIT_FAILURE);
}

/**
 * remove the brackets and replace commas by white-spaces
 */
void prepare_selection(char **selection)
{
	int length = strlen(*selection);
	for (int i = 0; i < length + 1; i++)
	{
		if ((*selection)[i + 1] == ',')
			(*selection)[i] = ' ';
		else
			(*selection)[i] = (*selection)[i + 1];
	}
	// Replace last character before null with null, a bit of a mouthful I know
	(*selection)[strlen(*selection)-1] = '\0';
}

// return if selection is in format [_,*]
bool first_underslash(char *selection)
{
	// After calling prepare_selection it should be on index 0 if at all
	return selection[0] == '_';
}

// return if selection is in format [*,_]
bool second_underslash(char *selection)
{
	int length = strlen(selection);
	// It should be the index after the first whitespace if at all
	for (int i = 1; i < length; i++)
		if (selection[i] == ' ' && i < length && selection[i + 1] == '_')
			return true;
	return false;
}

// return if selection is in format [find .*]
bool is_find_selection(char *selection)
{
	int length = strlen(selection);

	// Check if it is even long enough
	if (length <= FIND_LEN)
		return false;

	// Check if it begins with "find "
	char test[FIND_LEN + 1];
	memcpy(test, selection, FIND_LEN * sizeof(char));
	if (strcmp("find ", test) != 0)
		return false;

	if (!single_word(test, FIND_LEN))
		return false;

	return true;
}

// if in format [find STR], make selection into just STR
void create_find_selection(char **selection)
{
	int length = strlen(*selection);
	for (int i = FIND_LEN; i < length + 1; i++)
		(*selection)[i-FIND_LEN] = (*selection)[i];
}

bool is_box_selection(char *cmd, selection_t *sel)
{
	char *endptr1 = NULL, *endptr2 = NULL, *endptr3 = NULL;
	int first = strtol(cmd, &endptr1, 10);
	int second = strtol(endptr1, &endptr2, 10);
	int third, fourth;
	if (endptr2[1] == '-')
	{
		third = SLASH;
		// Remove ' -' from the beginning
		int length = strlen(endptr2);
		for (int i = 0; i != length - 1; i++)
			endptr2[i] = endptr2[i + 2];
		endptr3 = endptr2;
	}
	else
		third = strtol(endptr2, &endptr3, 10);

	if (endptr3[1] == '-')
		fourth = SLASH;
	else
		fourth = strtol(endptr3, NULL, 10);

	if (!first || !second || !third || !fourth)
		return false;
	else
	{
		sel->row1 = first;
		sel->col1 = second;
		sel->row2 = third;
		sel->col2 = fourth;
		return true;
	}
}


/**
 * If cmd is selection (returns true of is_selection), load information into the
 * selection_t structure and return it.
 * If there were any problems while parsing return type=INVALID_S
 */
selection_t load_selection_info(char *select_str, call_t *call)
{
	selection_t s =
	{
		.type=INVALID_S, .row1=0, .col1=0,
		.row2=0, .col2=0, .str=NULL
	};
	// Restoring temporary variable must be checked prior
	if(strcmp(select_str, "[_]") == 0)
	{
		s.type=TMP_VAR;
		return s;
	}
	if(strcmp(select_str, "[_,_]") == 0)
	{
		s.type=TABLE;
		return s;
	}
	int no_commas = get_no_commas(select_str);
	prepare_selection(&select_str);
	char *endptr1 = NULL, *endptr2 = NULL;
	bool f_uslash = false; // if first under-slash was found
	// This has to be done before, since if it were not a number strtol would
	// not work as I would like it to, replace it with 0 which is invalid
	if (first_underslash(select_str))
	{
		select_str[0] = '0';
		f_uslash = true;
	}
	s.row1 = strtol(select_str, &endptr1, 10);
	s.col1 = strtol(endptr1, &endptr2, 10);
	if (is_box_selection(select_str, &s) && no_commas == 3)
		s.type=BOX;
	else if (s.row1 > 0 && s.col1 > 0 && no_commas == 1)
		s.type=CELL;
	else if (s.row1 > 0 && s.col1 == 0 && second_underslash(select_str)
			&& no_commas == 1)
	{
		s.type=ROW;
	}
	else if (s.row1 == 0 && s.col1 > 0 && f_uslash && no_commas == 1)
		s.type=COL;
	else if (strcmp(select_str, "min") == 0)
		s.type=MIN;
	else if (strcmp(select_str, "max") == 0)
		s.type=MAX;
	else if(is_find_selection(select_str))
	{
		create_find_selection(&select_str);
		s.type = STR;
		int length = strlen(select_str);
		char *arg_str = calloc(length, sizeof(char));
		if (arg_str == NULL)
			alloc_fail_call(call);
		strcpy(arg_str, select_str);
		s.str = arg_str;
	}
	return s;
}

command_t load_modification_info(char *cmd, call_t *call)
{
	command_t cmd_s = { .type = MODIFICATION, .cmd_name = "", .cmd_num = -1,
		.arg1 = 0, .arg2 = 0, .str = NULL, .selection = NULL };
	for (int i = 0; i < NO_MODS; i ++)
		if (strcmp(cmd, mod_list[i]) == 0)
				cmd_s.cmd_num = i;
	strcpy(cmd_s.cmd_name, cmd);
	cmd_s.selection = &call->selections[call->count_s - 1];
	return cmd_s;
}

command_t load_data_info(char *cmd, call_t *call)
{
	command_t cmd_s = { .type = DATA, .cmd_name = "", .arg1 = 0, .arg2 = 0,
		.str = NULL, .selection = NULL };
	// step 1 get cmd_name from cmd
	// if there are no selection error, otherwise get address of last
	cmd_s.selection = &call->selections[call->count_s - 1];
	for (int i = 0; i < NO_DATA; i++)
		if(begins_with(cmd, data_list[i]))
		{
				strcpy(cmd_s.cmd_name, data_list[i]);
				cmd_s.cmd_num = i;
		}

	// step 2 get arg1 and arg2 if cmd_name requires them
	if (strcmp(cmd_s.cmd_name, "set") != 0 &&
			strcmp(cmd_s.cmd_name, "clear") != 0)
	{
		int length = strlen(cmd);
		char *arg_str = calloc(length, sizeof(char));
		if (arg_str == NULL)
			alloc_fail_call(call);
		int start = strlen(cmd_s.cmd_name);
		// Copy just the [R,C] part into arg_str
		for (int i = start; i < length; i++)
			arg_str[i - start] = cmd[i];
		prepare_selection(&arg_str);
		char *endptr1;
		cmd_s.arg1 = strtol(arg_str, &endptr1, 10);
		cmd_s.arg2 = strtol(endptr1, NULL ,10);
		free(arg_str);
		if (cmd_s.arg1 < 0 || cmd_s.arg2 < 0)
		{
			call_dtor(call);
			fprintf(stderr, "Invalid argument!\n");
			exit(EXIT_FAILURE);
		}
	}
	// step 3 get STR if cmd_name requires them
	if (strcmp(cmd_s.cmd_name, "set") == 0)
	{
		int length = strlen(cmd);
		char *arg_str = calloc(length, sizeof(char));
		if (arg_str == NULL)
			alloc_fail_call(call);
		int start = strlen(cmd_s.cmd_name) + 1;
		// copy STR into arg_str
		for (int i = start; i < length; i++)
			arg_str[i - start] = cmd[i];
		cmd_s.str = arg_str;
	}
	return cmd_s;
}

command_t load_var_info(char *cmd, call_t *call)
{
	command_t cmd_s = { .type = VARIABLE, .cmd_name = "", .cmd_num = 0,
		.arg1 = 0, .arg2 = 0, .str = NULL, .selection = NULL };
	strcpy(cmd_s.cmd_name, cmd);
	if (begins_with(cmd, "def _"))
	{
		// set last selection
		cmd_s.selection = &call->selections[call->count_s - 1];
		cmd_s.cmd_num = DEF;
	}
	else if (begins_with(cmd, "use _"))
	{
		// set last selection
		cmd_s.selection = &call->selections[call->count_s - 1];
		cmd_s.cmd_num = USE;
	}
	else if (begins_with(cmd, "inc _"))
	{
		cmd_s.cmd_num = INC;
	}

	if (strcmp(cmd, "[set]") == 0)
	{
		cmd_s.selection = &call->selections[call->count_s - 1];
		cmd_s.cmd_num = SET_VAR;
	}
	return cmd_s;
}

void unkown_command(call_t *call, char *current)
{
	fprintf(stderr, "Unknown command given!\n");
	call_dtor(call);
	free(current);
	exit(EXIT_FAILURE);
}

/**
 * Remember to always free the array!
 * @return array of cmd_types_t of all commands parsed
 */
void load_command(char *cmd, int *no_commands, call_t *call)
{
	int size = CHUNK;
	char *current = calloc(size, sizeof(char));
	if (current == NULL)
		alloc_fail_call(call);
	int curr_i = 0;
	// Add default selection [1,1]
	selection_t new =
	{
		.type=CELL, .row1=1, .col1=1,
		.row2=0, .col2=0, .str=NULL
	};
	call_add_selection(call, new);
	int length = strlen(cmd);
	for (int i = 0; i <= length; i++)
	{
		if (cmd[i] == ';' || i == length)
		{
			current[curr_i] = '\0';
			// Skip empty commands ';;' for example
			if (strlen(current) == 0)
				continue;
			curr_i = 0;
			cmd_types_t cmd_type = get_command_type(current);
			if (cmd_type == INVALID)
				unkown_command(call, current);
			*no_commands+=1;
			if (cmd_type == SELECTION)
			{
				selection_t scurent = load_selection_info(current, call);
				if (scurent.type == INVALID_S)
					unkown_command(call, current);
				call_add_selection(call, scurent);
			}
			else if(cmd_type == MODIFICATION)
			{
				command_t ccurrent = load_modification_info(current, call);
				call_add_cmd(call, ccurrent);
			}
			else if(cmd_type == DATA)
			{
				command_t ccurrent = load_data_info(current, call);
				call_add_cmd(call, ccurrent);
			}
			else if (cmd_type == VARIABLE)
			{
				command_t ccurent = load_var_info(current, call);
				call_add_cmd(call, ccurent);
			}
		}
		else // if not ';'
			current[curr_i++] = cmd[i];
	}
	free(current);
}

/**
 * Check if is a valid file, has non-NULL filename and fopen succeeded
 * @param FILE *file - file to check
 * @param char *file_name - file name to check
 * @return boolean - true if everything is OK
 */
bool check_file(FILE *file, char *file_name)
{
	if (file_name == NULL)
	{
		fprintf(stderr, "File not given!\n");
		return false;
	}
	if (file == NULL)
	{
		fprintf(stderr, "File %s could not be opened!\n", file_name);
		return false;
	}
	return true;
}

bool cmd_parse(char *cmd, int *no_cmd, call_t *call)
{
	if (cmd == NULL)
	{
		fprintf(stderr, "No command given!\n");
		call_dtor(call);
		return false;
	}
	load_command(cmd, no_cmd, call);
	if (*no_cmd < 1)
	{
		fprintf(stderr, "Not enough commands given!\n");
		call_dtor(call);
		return false;
	}
	return true;
}

/**
 * Create, fill, print and destroy a table
 * @param FILE *file - file to use for content
 * @param char *delim - what to use as delimiter
 * @return boolean - true if everything went OK
 */
bool table_handling(FILE *file, char *delim, table_t *table)
{
	int cols = 0;
	int rows = get_sizes(file, delim, &cols);
	*table = table_ctor(rows, cols, file);
	fill_table_with_data(table, file, delim);
	fclose(file);
	return true;
}


// Row manipulation
void move_row(table_t *table, int index_from, int index_to)
{
	if (index_from > index_to)
	{
		while (index_from > index_to)
		{
			row_swap(&table->rows[index_from], &table->rows[index_from - 1]);
			index_from--;
		}
	}
	else
	{
		while (index_from < index_to)
		{
			row_swap(&table->rows[index_from], &table->rows[index_from + 1]);
			index_from++;
		}
	}
}

void add_row_before(table_t *table, int row)
{
	table_add_rows(table, 1);
	move_row(table, table->no_rows - 1, row);

}

void add_row_after(table_t *table, int row)
{
	table_add_rows(table, 1);
	move_row(table, table->no_rows - 1, row + 1);
}

void delete_row(table_t *table, int row)
{
	move_row(table, row, table->no_rows - 1);
	table_delete_row(table);
}

// Column manipulation
void move_col(table_t *table, int index_from, int index_to)
{
	for (int i = 0; i < table->no_rows; i++)
	{
		int local_from = index_from;
		int local_to = index_to;
		if (local_from > local_to)
		{
			while (local_from > local_to)
			{
				col_swap(&table->rows[i].cols[local_from],
						&table->rows[i].cols[local_from - 1]);
				local_from--;
			}
		}
		else
		{
			while (local_from < local_to)
			{
				col_swap(&table->rows[i].cols[local_from],
						&table->rows[i].cols[local_from + 1]);
				local_from++;
			}
		}
	}
}

void add_col_before(table_t *table, int col)
{
	table_add_cols(table, 1);
	move_col(table, table->rows[0].no_cols - 1, col);

}

void add_col_after(table_t *table, int col)
{
	table_add_cols(table, 1);
	move_col(table, table->rows[0].no_cols - 1, col + 1);
}

void delete_col(table_t *table, int col)
{
	move_col(table, col, table->rows[0].no_cols - 1);
	table_delete_col(table);
}

/*
 * Call Processing
 */
void irow(table_t *table, command_t cmd)
{
	int row = cmd.selection->row1 - 1;
	if (cmd.selection->type == CELL || cmd.selection->type == ROW)
		add_row_before(table, row);
	else if (cmd.selection->type == COL || cmd.selection->type == TABLE)
	{
		// This is quite a performance intensive solution, easy to understand
		// however, Skip every other row since there will be one new empty row
		for (int i = 0; i < table->no_rows; i += 2)
			add_row_before(table, i);
	}
	else if (cmd.selection->type == BOX)
	{
		int row_start = cmd.selection->row1 - 1;
		int row_end = cmd.selection->row2;
		if (row_end == SLASH)
			row_end = table->no_rows - 1;
		else
			row_end--; // the -1 missing in declaration unlike row_start
		int max_step = row_start + 2 * (row_end - row_start) + 1;
		// max_step is index right after where the last row should be added
		// 2 * (row_end - row_start) since we care about distance between the
		// two times two since empty spaces will be added + 1 to place it right
		// after
		for (int i = row_start; i < max_step; i += 2)
			add_row_before(table, i);
	}
}

void arow(table_t *table, command_t cmd)
{
	int row = cmd.selection->row1 - 1;
	if (cmd.selection->type == CELL || cmd.selection->type == ROW)
		add_row_after(table, row);
	else if (cmd.selection->type == COL || cmd.selection->type == TABLE)
	{
		// This is quite a performance intensive solution, easy to understand
		// however, Skip every other row since there will be one new empty row
		for (int i = 0; i < table->no_rows; i += 2)
			add_row_after(table, i);
	}
	else if (cmd.selection->type == BOX)
	{
		int row_start = cmd.selection->row1 - 1;
		int row_end = cmd.selection->row2;
		if (row_end == SLASH)
			row_end = table->no_rows - 1;
		else
			row_end--; // the -1 missing in declaration unlike row_start
		int max_step = row_start + 2 * (row_end - row_start) + 1;
		// Explanation same as irow
		for (int i = row_start; i < max_step; i += 2)
			add_row_after(table, i);
	}
}

void drow(table_t *table, command_t cmd)
{
	int row = cmd.selection->row1 - 1;
	if (cmd.selection->type == CELL || cmd.selection->type == ROW)
		delete_row(table, row);
	else if (cmd.selection->type == COL || cmd.selection->type == TABLE)
		table_dtor(table);
	else if (cmd.selection->type == BOX)
	{
		int row_start = cmd.selection->row1 - 1;
		int row_end = cmd.selection->row2;
		if (row_end == SLASH)
			row_end = table->no_rows - 1;
		else
			row_end--; // the -1 missing in declaration unlike row_start
		int max_step = row_start + row_end - row_start + 1;
		// Same as irow but there won't be new extra empty lines
		for (int i = row_start; i < max_step; i++)
			delete_row(table, row_start);
	}
}

void icol(table_t *table, command_t cmd)
{
	int col = cmd.selection->col1 - 1;
	if (cmd.selection->type == CELL || cmd.selection->type == COL)
		add_col_before(table, col);
	else if (cmd.selection->type == ROW || cmd.selection->type == TABLE)
	{
		for (int i = 0; i < table->rows[0].no_cols; i +=2)
			add_col_before(table, i);
	}
	else if (cmd.selection->type == BOX)
	{
		int col_start = cmd.selection->col1 - 1;
		int col_end = cmd.selection->col2;
		if (col_end == SLASH)
			col_end = table->rows[0].no_cols - 1;
		else
			col_end--; // the -1 missing in declaration unlike col_start
		int max_step = col_start + 2 * (col_end - col_start) + 1;
		for (int i = col_start; i < max_step; i +=2)
			add_col_before(table, i);
	}
}

void acol(table_t *table, command_t cmd)
{
	int col = cmd.selection->col1 - 1;
	if (cmd.selection->type == CELL || cmd.selection->type == COL)
		add_col_after(table, col);
	else if (cmd.selection->type == ROW || cmd.selection->type == TABLE)
	{
		for (int i = 0; i < table->rows[0].no_cols; i +=2)
			add_col_after(table, i);
	}
	else if (cmd.selection->type == BOX)
	{
		int col_start = cmd.selection->col1 - 1;
		int col_end = cmd.selection->col2;
		if (col_end == SLASH)
			col_end = table->rows[0].no_cols - 1;
		else
			col_end--; // the -1 missing in declaration unlike col_start
		int max_step = col_start + 2 * (col_end - col_start) + 1;
		for (int i = col_start; i < max_step; i +=2)
			add_col_after(table, i);
	}
}

void dcol(table_t *table, command_t cmd)
{
	int col = cmd.selection->col1 - 1;
	if (cmd.selection->type == CELL || cmd.selection->type == COL)
		delete_col(table, col);
	else if (cmd.selection->type == ROW || cmd.selection->type == TABLE)
		table_dtor(table);
	else if (cmd.selection->type == BOX)
	{
		int col_start = cmd.selection->col1 - 1;
		int col_end = cmd.selection->col2;
		if (col_end == SLASH)
			col_end = table->rows[0].no_cols - 1;
		else
			col_end--; // the -1 missing in declaration unlike col_start
		int max_step = col_start + col_end - col_start + 1;
		// Same as icol but there won't be new extra empty lines
		for (int i = col_start; i < max_step; i++)
			delete_col(table, col_start);
	}
}

void process_mods(table_t *table, command_t cmd)
{
	void (*func_ptr[]) (table_t*, command_t) = { irow, arow, drow, icol, acol,
		dcol };
	(*func_ptr[cmd.cmd_num]) (table, cmd);
}


void set_selection(table_t *table, selection_t *selection, char *value)
{
	stype_t stype = selection->type;
	int row1 = selection->row1 - 1;
	int col1 = selection->col1 - 1;
	// Other stypes than those 4 should not occur at all thanks to apply_call
	// checks function, hence the else if instead of switch, since it is ENUM
	if (stype == CELL)
		set_cell_value(table, row1, col1, value, NULL);
	else if (stype == ROW)
		for (int i = 0; i < table->rows[row1].no_cols; i++)
			set_cell_value(table, row1, i, value, NULL);
	else if (stype == COL)
		for (int i = 0; i < table->no_rows; i++)
			set_cell_value(table, i, col1, value, NULL);
	else if (stype == BOX)
	{
			if (selection->row2 == SLASH)
				selection->row2 = table->no_rows;
			if (selection->col2 == SLASH)
				selection->col2 = table->rows[0].no_cols;
			for (int i = row1; i < selection->row2; i++)
				for (int j = col1; j < selection->col2; j++)
					set_cell_value(table, i, j, value, NULL);
	}
	else if (stype == TABLE)
		for (int i = 0; i < table->no_rows; i++)
			for (int j = 0; j < table->rows[i].no_cols; j++)
				set_cell_value(table, i, j, value, NULL);
}

void set(table_t *table, command_t cmd, call_t *call)
{
	unescape_string(cmd.str, table, call);
	(void)call;
	set_selection(table, cmd.selection, cmd.str);
}

void clear(table_t *table, command_t cmd, call_t *call)
{
	(void)call;
	set_selection(table, cmd.selection, "");
}

void swap(table_t *table, command_t cmd, call_t *call)
{
	(void)call;
	int row1 = cmd.selection->row1 - 1;
	int col1 = cmd.selection->col1 - 1;
	int row2 = cmd.arg1 - 1;
	int col2 = cmd.arg2 - 1;
	stype_t stype = cmd.selection->type;
	if (stype == CELL)
		col_swap(&table->rows[row1].cols[col1],
				&table->rows[row2].cols[col2]);
	else if (stype == ROW)
		for (int i = 0; i < table->rows[row1].no_cols; i++)
		{
			col_swap(&table->rows[row1].cols[i],
				&table->rows[row2].cols[col2]);
		}
	else if (stype == COL)
		for (int i = 0; i < table->no_rows; i++)
		{
			col_swap(&table->rows[i].cols[col1],
				&table->rows[row2].cols[col2]);
		}
	else if (stype == BOX)
	{
			if (cmd.selection->row2 == SLASH)
				cmd.selection->row2 = table->no_rows;
			if (cmd.selection->col2 == SLASH)
				cmd.selection->col2 = table->rows[0].no_cols;
			for (int i = row1; i < cmd.selection->row2; i++)
			{
				for (int j = col1; j < cmd.selection->col2; j++)
				{
					col_swap(&table->rows[i].cols[j],
						&table->rows[row2].cols[col2]);
				}
			}
	}
	else if (stype == TABLE)
	{
		for (int i = 0; i < table->no_rows; i++)
		{
			for (int j = 0; j < table->rows[i].no_cols; j++)
				{
					col_swap(&table->rows[i].cols[j],
						&table->rows[row2].cols[col2]);
				}
		}
	}
}

double selection_sum(table_t *table, selection_t *selection, int *no_additions)
{
	stype_t stype = selection->type;
	int row1 = selection->row1 - 1;
	int col1 = selection->col1 - 1;
	double sum = 0.0;
	*no_additions = 0;
	double current = 0.0;
	if (stype == CELL)
	{
		get_cell_numeric(table, row1, col1, &current);
		sum += current;
		*no_additions+=1;
	}
	else if (stype == ROW)
	{
		for (int i = 0; i < table->rows[row1].no_cols; i++)
		{
			if (get_cell_numeric(table, row1, i, &current))
				*no_additions+=1;
			sum += current;
		}
	}
	else if (stype == COL)
	{
		for (int i = 0; i < table->no_rows; i++)
		{
			if (get_cell_numeric(table, i, col1, &current))
				*no_additions+=1;
			sum += current;
		}
	}
	else if (stype == BOX)
	{
		if (selection->row2 == SLASH)
			selection->row2 = table->no_rows;
		if (selection->col2 == SLASH)
			selection->col2 = table->rows[0].no_cols;
		for (int i = row1; i < selection->row2; i++)
		{
			for (int j = col1; j < selection->col2; j++)
			{
				if (get_cell_numeric(table, i, j, &current))
					*no_additions+=1;
				sum += current;
			}
		}
	}
	else if (stype == TABLE)
	{
		for (int i = 0; i < table->no_rows; i++)
		{
			for (int j = 0; j < table->rows[i].no_cols; j++)
			{
				if (get_cell_numeric(table, i, j, &current))
					*no_additions+=1;
				sum += current;
			}
		}
	}
	return sum;
}

void sum(table_t *table, command_t cmd, call_t *call)
{
	int useless;
	double sum = selection_sum(table, cmd.selection, &useless);
	int alloc_size = snprintf(NULL, 0, "%g", sum) + 1;
	char *text = malloc(alloc_size * sizeof(char));
	if (text == NULL)
		alloc_fail_table_call(table, call);
	sprintf(text, "%g", sum);
	set_cell_value(table, cmd.arg1 - 1, cmd.arg2 - 1, text, text);
	free(text);
}

void avg(table_t *table, command_t cmd, call_t *call)
{
	int count = 0;
	double sum = selection_sum(table, cmd.selection, &count);
	sum/=count;
	int alloc_size = snprintf(NULL, 0, "%g", sum) + 1;
	char *text = malloc(alloc_size * sizeof(char));
	if (text == NULL)
		alloc_fail_table_call(table, call);
	sprintf(text, "%g", sum);
	set_cell_value(table, cmd.arg1 - 1, cmd.arg2 - 1, text, text);
	free(text);
}

void count(table_t *table, command_t cmd, call_t *call)
{
	int store_row = cmd.arg1 - 1;
	int store_col = cmd.arg2 - 1;
	stype_t stype = cmd.selection->type;
	int row1 = cmd.selection->row1 - 1;
	int col1 = cmd.selection->col1 - 1;
	int non_empty = 0;
	if (stype == CELL)
	{
		if (table->rows[row1].cols[col1].length != 0)
			non_empty++;
	}
	else if (stype == ROW)
	{
		for (int i = 0; i < table->rows[row1].no_cols; i++)
			if (table->rows[row1].cols[i].length != 0)
				non_empty++;
	}
	else if (stype == COL)
	{
		for (int i = 0; i < table->no_rows; i++)
			if (table->rows[i].cols[col1].length != 0)
				non_empty++;
	}
	else if (stype == BOX)
	{
		if (cmd.selection->row2 == SLASH)
			cmd.selection->row2 = table->no_rows;
		if (cmd.selection->col2 == SLASH)
			cmd.selection->col2 = table->rows[0].no_cols;
		for (int i = row1; i < cmd.selection->row2; i++)
			for (int j = col1; j < cmd.selection->col2; j++)
				if (table->rows[i].cols[j].length != 0)
					non_empty++;
	}
	else if (stype == TABLE)
	{
		for (int i = 0; i < table->no_rows; i++)
			for (int j = 0; j < table->rows[i].no_cols; j++)
				if (table->rows[i].cols[j].length != 0)
					non_empty++;
	}

	int alloc_size = snprintf(NULL, 0, "%d", non_empty) + 1;
	char *text = malloc(alloc_size * sizeof(char));
	if (text == NULL)
		alloc_fail_table_call(table, call);
	sprintf(text, "%d", non_empty);
	set_cell_value(table, store_row, store_col, text, text);
	free(text);
}

void len(table_t *table, command_t cmd, call_t *call)
{
	int store_row = cmd.arg1 - 1;
	int store_col = cmd.arg2 - 1;

	int row1 = cmd.selection->row1 - 1;
	int col1 = cmd.selection->col1 - 1;
	int row2 = cmd.selection->row2 - 1;
	int col2 = cmd.selection->col2 - 1;
	int len = 0;
	stype_t stype = cmd.selection->type;
	if (stype == CELL)
		len = strlen(get_cell_content(table, row1, col1));
	else if (stype == ROW)
		len = strlen(get_cell_content(table, row1, table->rows[0].no_cols - 1));
	else if (stype == COL)
		len = strlen(get_cell_content(table, table->no_rows - 1, col1));
	else if (stype == BOX)
		len = strlen(get_cell_content(table, row2, col2));
	else if (stype == TABLE)
		len = strlen(get_cell_content(table, table->no_rows -1 ,
					table->rows[0].no_cols - 1));

	int alloc_size = snprintf(NULL, 0, "%d", len) + 1;
	char *text = malloc(alloc_size * sizeof(char));
	if (text == NULL)
		alloc_fail_table_call(table, call);
	sprintf(text, "%d", len);
	set_cell_value(table, store_row, store_col, text, text);
	free(text);
}

void process_data(table_t *table, command_t cmd, call_t *call)
{
	void (*func_ptr[]) (table_t*, command_t, call_t*) = { set, clear, swap, sum,
		avg, count, len };
	(*func_ptr[cmd.cmd_num]) (table, cmd, call);
}

// Return index (last character) as a number
int get_var_index(command_t cmd)
{
	return (int) cmd.cmd_name[strlen(cmd.cmd_name) - 1] - '0';
}

void def(table_t *table, command_t cmd, variables_t *vars)
{
	int index = get_var_index(cmd);
	int row = cmd.selection->row1 - 1;
	int col = cmd.selection->col1 - 1;
	char *value = get_cell_content(table, row, col);
	variable_store(table, vars, index, value);
}

void use(table_t *table, command_t cmd, variables_t *vars)
{
	set_selection(table, cmd.selection, vars->values[get_var_index(cmd)]);
}

void inc(table_t *table, command_t cmd, variables_t *vars)
{
	int index = get_var_index(cmd);
	char *endptr = NULL;
	int ret = strtol(vars->values[index], &endptr, 10);
	if (endptr == NULL)
		ret = 1;
	else
		ret++;

	int alloc_size = snprintf(NULL, 0, "%d", ret) + 1;
	char *temp = malloc(alloc_size * sizeof(char));
	 if (temp == NULL)
		alloc_fail_table(table);
	sprintf(temp, "%d", ret);
	variable_store(table, vars, index, temp);
	free(temp); // If variable_store fails it will leak!
}

void set_var(table_t *table, command_t cmd, variables_t *vars)
{
	(void)table; // So that all var functions have the same type
	vars->selection = cmd.selection;
}


void process_var(table_t *table, command_t cmd, variables_t *vars)
{
	void (*func_ptr[]) (table_t*, command_t, variables_t*) = { def, use, inc,
		set_var };
	(*func_ptr[cmd.cmd_num]) (table, cmd, vars);
}

// Increase number of rows or cols if selections need it
void table_expand(table_t *table, command_t command)
{
	int diff_r = 0; // Number of rows to add
	int diff_c = 0; // Number of columns to add
	int current = 0;

	if (command.selection != NULL)
	{
		if (command.selection->row1 > table->no_rows)
		{
			current = command.selection->row1 - table->no_rows;
			if (current > diff_r)
				diff_r = current;
		}
		if (command.selection->row2 > table->no_rows)
		{
			current = command.selection->row2 - table->no_rows;
			if (current > diff_r)
				diff_r = current;
		}
		if (command.selection->col1 > table->rows[0].no_cols)
		{
			current = command.selection->col1 - table->rows[0].no_cols;
			if (current > diff_c)
				diff_c = current;
		}
		if (command.selection->col2 > table->rows[0].no_cols)
		{
			current = command.selection->col2 - table->rows[0].no_cols;
			if (current > diff_c)
				diff_c = current;
		}
	}

	if (command.arg1 > table->no_rows)
	{
		current = command.arg1 - table->no_rows;
		if (current > diff_r)
			diff_r = current;
	}
	if (command.arg2 > table->rows[0].no_cols)
	{
		current = command.arg2 - table->rows[0].no_cols;
		if (current > diff_c)
			diff_c = current;
	}

	if (diff_r > 0)
		table_add_rows(table, diff_r);
	if (diff_c > 0)
		table_add_cols(table, diff_c);
}

void no_match_error(table_t *table, call_t *call, variables_t *vars)
{
	fprintf(stderr, "No match for selection!\n");
	table_dtor(table);
	call_dtor(call);
	variables_dtor(vars);
	exit(EXIT_FAILURE);
}

/**
 * make changes to the table according to call
 */
bool apply_call(table_t *table, call_t *call, variables_t *vars)
{
	for (int i = 0; i < call->count_c; i++)
	{
		selection_t *selection = call->commands[i].selection;
		selection_t *first = &call->selections[0];
		stype_t stype = INVALID_S;
		if (selection != NULL)
			stype = selection->type;
		selection_t new =
		{
			.type=INVALID_S, .row1=0, .col1=0,
			.row2=0, .col2=0, .str=NULL
		};
		// Replace MIN, MAX and STR selections by CELL
		// Replace TMP_VAR by stored selection
		if (stype == MIN)
		{
			// a bit of nasty pointer arithmetic, compensating for bad design,
			// sadly do not have time to rewrite, selection before current one
			if(find_min_cell(table, call->selections[selection - first - 1], &new))
				call->commands[i].selection = &new;
			else
				no_match_error(table, call, vars);
		}
		if (stype == MAX)
		{
			if(find_max_cell(table, call->selections[selection - first - 1], &new))
				call->commands[i].selection = &new;
			else
				no_match_error(table, call, vars);
		}
		if (stype == TMP_VAR)
			call->commands[i].selection = vars->selection;
		if (stype == STR)
		{
			if (find_substr_cell(table, call->selections[selection - first - 1],
						*selection, &new, call))
				call->commands[i].selection = &new;
			else
				no_match_error(table, call, vars);
		}
		table_expand(table, call->commands[i]);
		switch(call->commands[i].type)
		{
			case SELECTION:
				break;
			case MODIFICATION:
				process_mods(table, call->commands[i]);
				break;
			case DATA:
				process_data(table, call->commands[i], call);
				break;
			case VARIABLE:
				process_var(table, call->commands[i], vars);
				break;
			case INVALID:
				// Will not occur
				break;
		}
	}
	return true;
}

// Remove empty trailing columns
void table_trim(table_t *table)
{
	bool non_empty = false;
	// Go through all columns in reverse order if non empty break, else delete
	if (table->no_rows == 0)
		return;
	for (int i = table->rows[0].no_cols - 1; i > -1; i--)
	{
		for (int j = 0; j < table->no_rows; j++)
		{
			if (table->rows[j].cols[i].length != 0)
			{
				non_empty = true;
				break;
			}
		}
		if (non_empty)
			break;
		else
			table_delete_col(table);
	}
}

int main(int argc, char **argv)
{
	char *delim=" ";
	char *file_name = NULL;
	int command_found = 0;
	char *cmd = NULL;
	int no_cmd = 0;
	call_t call = call_ctor();
	table_t table;

	if (argc < 2)
	{
		fprintf(stderr, "Not enough arguments!\n");
		return EXIT_FAILURE;
	}

	for (int i = 1; i < argc; i++)
	{
		if (strcmp("-d", argv[i]) == 0)
		{
			if (i == argc - 1)
			{
				fprintf(stderr, "Delimiter not given!\n");
				return EXIT_FAILURE;
			}
			delim = argv[i + 1];
			i++;
			if (!valid_delim(delim))
			{
				fprintf(stderr, "Delimiter contains an invalid character!\n");
				return EXIT_FAILURE;
			}
		}
		else if (!command_found++)
			cmd = argv[i];
		else
			file_name = argv[i];
	}

	call.delim = delim;

	if (!cmd_parse(cmd, &no_cmd, &call))
		return EXIT_FAILURE;

	FILE *file = fopen(file_name, "r");

	if (!check_file(file, file_name))
		return EXIT_FAILURE;

	if (!table_handling(file, delim, &table))
		return EXIT_FAILURE;

	variables_t vars = variables_ctor(&call);
	apply_call(&table, &call, &vars);

	table_trim(&table);
	file = fopen(file_name, "w");
	if (!check_file(file, file_name))
		return EXIT_FAILURE;
	write_table(file, table, delim);
	fclose(file);

	variables_dtor(&vars);
	table_dtor(&table);
	call_dtor(&call);

	return EXIT_SUCCESS;
}
