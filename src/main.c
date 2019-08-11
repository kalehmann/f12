#include <argp.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "f12.h"

enum f12_command {
	COMMAND_NONE,
	COMMAND_CREATE,
	COMMAND_DEL,
	COMMAND_GET,
	COMMAND_INFO,
	COMMAND_LIST,
	COMMAND_MOVE,
	COMMAND_PUT,
};

#define NUMBER_OF_COMMANDS 7

enum opts {
	OPT_CREATE_ROOT_DIR = 256,
	OPT_CREATE_VOLUME_LABEL,
	OPT_CREATE_SIZE,
	OPT_CREATE_SECTOR_SIZE,
	OPT_CREATE_SECTORS_PER_CLUSTER,
	OPT_CREATE_RESERVED_SECTORS,
	OPT_CREATE_NUMBER_OF_FATS,
	OPT_CREATE_ROOT_DIR_ENTRIES,
	OPT_CREATE_DRIVE_NUMBER,
	OPT_DEL_SOFT_DELETE,
	OPT_INFO_DUMP_BPB,
	OPT_LIST_WITH_SIZE,
};

const char *argp_program_version = "0.0";
const char *argp_program_bug_address = "Karsten Lehmann <mail@kalehmann.de>";

struct arguments {
	struct f12_create_arguments *create_arguments;
	struct f12_del_arguments *del_arguments;
	struct f12_get_arguments *get_arguments;
	struct f12_info_arguments *info_arguments;
	struct f12_list_arguments *list_arguments;
	struct f12_move_arguments *move_arguments;
	struct f12_put_arguments *put_arguments;
	char *device_path;
	int recursive;
	int verbose;
	enum f12_command command;
};

static long int parse_long(char *str)
{
	char *endptr = NULL;
	long int temp = 0;
	int base;

	if (0 == strncmp("0x", str, 2)) {
		base = 16;
	} else if (0 == strncmp("0b", str, 2)) {
		base = 2;
	} else if (strlen(str) && str[0] == '0') {
		base = 8;
	} else {
		base = 10;
	}

	temp = strtol(str, &endptr, base);

	if (errno != 0) {
		fprintf(stderr,
			"An error occurred while parsing %s as a number: %s\n",
			str, strerror(errno));
		exit(EXIT_FAILURE);
	} else if (endptr == str || *endptr != 0) {
		fprintf(stderr, "Could not parse %s as number\n", str);
		exit(EXIT_FAILURE);
	}

	return temp;
}

static int power_of_two(long n)
{
	int nonzeroBits = 0;
	while (n) {
		nonzeroBits += (n & 1);
		n = n >> 1;
	}

	return nonzeroBits == 1;
}

error_t parser_create(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;
	struct f12_create_arguments *create_arguments = args->create_arguments;
	long int temp = 0;

	switch (key) {
	case (ARGP_KEY_ARG):
		if (NULL != create_arguments->root_dir_path) {
			argp_usage(state);
		}
		create_arguments->root_dir_path = arg;

		return 0;
	case (OPT_CREATE_ROOT_DIR):
		create_arguments->root_dir_path = arg;

		return 0;
	case (OPT_CREATE_VOLUME_LABEL):
		create_arguments->volume_label = arg;

		return 0;
	case (OPT_CREATE_SIZE):
		temp = parse_long(arg);
		if (temp < 10 || temp > 32768) {
			fprintf(stderr, "Invalid size\n");
			exit(EXIT_FAILURE);
		}
		create_arguments->volume_size = (uint16_t) temp;

		return 0;
	case (OPT_CREATE_SECTOR_SIZE):
		temp = parse_long(arg);
		if (512 > temp || 4096 < temp || !power_of_two(temp)) {
			fprintf(stderr, "Sector size %s out of range\n", arg);
			exit(EXIT_FAILURE);
		}
		create_arguments->sector_size = (uint16_t) temp;

		return 0;
	case (OPT_CREATE_SECTORS_PER_CLUSTER):
		temp = parse_long(arg);
		if (!power_of_two(temp) || temp > 128) {
			fprintf(stderr, "Sectors per cluster out of range\n");
			exit(EXIT_FAILURE);
		}
		create_arguments->sectors_per_cluster = (uint16_t) temp;

		return 0;
	case (OPT_CREATE_RESERVED_SECTORS):
		temp = parse_long(arg);
		if (temp < 1 || temp > 65535) {
			fprintf(stderr, "Reserved sectors out of range\n");
			exit(EXIT_FAILURE);
		}
		create_arguments->reserved_sectors = (uint16_t) temp;

		return 0;
	case (OPT_CREATE_NUMBER_OF_FATS):
		temp = parse_long(arg);
		if (temp < 1 || temp > 2) {
			fprintf(stderr,
				"Invalid number of file allocation tables\n");
			exit(EXIT_FAILURE);
		}
		create_arguments->number_of_fats = (uint8_t) temp;

		return 0;
	case (OPT_CREATE_ROOT_DIR_ENTRIES):
		temp = parse_long(arg);
		if (temp != 64 && temp != 112 && temp != 224 && temp != 512) {
			fprintf(stderr, "Invalid number of root dir entries\n");
			exit(EXIT_FAILURE);
		}
		create_arguments->root_dir_entries = (uint16_t) temp;

		return 0;
	case (OPT_CREATE_DRIVE_NUMBER):
		temp = parse_long(arg);
		create_arguments->drive_number = (uint8_t) temp;

		return 0;
	}

	return ARGP_ERR_UNKNOWN;
}

// *INDENT-OFF* 
static struct argp_option create_options[] = {
	{
		.name = "root-dir",
		.key = OPT_CREATE_ROOT_DIR,
		.arg = "PATH",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "volume-label",
		.key = OPT_CREATE_VOLUME_LABEL,
		.arg = "NAME",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "size",
		.key = OPT_CREATE_SIZE,
		.arg = "SIZE",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "sector-size",
		.key = OPT_CREATE_SECTOR_SIZE,
		.arg = "SIZE",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "sectors-per-cluster",
		.key = OPT_CREATE_SECTORS_PER_CLUSTER,
		.arg = "N",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "reserved-sectors",
		.key = OPT_CREATE_RESERVED_SECTORS,
		.arg = "N",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "number-of-fats",
		.key = OPT_CREATE_NUMBER_OF_FATS,
		.arg = "N",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "root-dir-entries",
		.key = OPT_CREATE_ROOT_DIR_ENTRIES,
		.arg = "N",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "drive-number",
		.key = OPT_CREATE_DRIVE_NUMBER,
		.arg = "NUMBER",
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{ 0 }
};
// *INDENT-ON*

static struct argp argp_create = {
	.options = create_options,
	.parser = parser_create,
	.args_doc = NULL,
	.doc = NULL,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL
};

error_t parser_del(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;
	struct f12_del_arguments *del_arguments = args->del_arguments;

	switch (key) {
	case (OPT_DEL_SOFT_DELETE):
		del_arguments->soft_delete = 1;

		return 0;
	case (ARGP_KEY_ARG):
		if (NULL != del_arguments->path) {
			argp_usage(state);
		}
		del_arguments->path = arg;

		return 0;
	}

	return ARGP_ERR_UNKNOWN;
}

// *INDENT-OFF*
static struct argp_option del_options[] = {
	{
		.name = "recursive",
		.key = 'r',
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "soft-delete",
		.key = OPT_DEL_SOFT_DELETE,
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{ 0 }
};
// *INDENT-ON*

static struct argp argp_del = {
	.options = del_options,
	.parser = parser_del,
	.args_doc = NULL,
	.doc = NULL,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL
};

error_t parser_get(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;
	struct f12_get_arguments *get_arguments = args->get_arguments;

	switch (key) {
	case (ARGP_KEY_ARG):
		if (NULL == get_arguments->path) {
			get_arguments->path = arg;

			return 0;
		}
		if (NULL == get_arguments->dest) {
			get_arguments->dest = arg;

			return 0;
		}

		argp_usage(state);
	}

	return ARGP_ERR_UNKNOWN;
}

// *INDENT-OFF*
static struct argp_option get_options[] = {
	{
		.name = "verbose",
		.key = 'v',
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{ 0 }
};
// *INDENT-ON*

static struct argp argp_get = {
	.options = get_options,
	.parser = parser_get,
	.args_doc = NULL,
	.doc = NULL,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL
};

error_t parser_info(int key, char *arg, struct argp_state *state)
{
	(void) arg;		// Suppress unused parameter warning
	struct arguments *args = state->input;
	struct f12_info_arguments *info_arguments = args->info_arguments;

	switch (key) {
	case (OPT_INFO_DUMP_BPB):
		info_arguments->dump_bpb = 1;

		return 0;
	}

	return ARGP_ERR_UNKNOWN;
}

// *INDENT-OFF*
static struct argp_option info_options[] = {
	{
		.name = "dump-bpb",
		.key = OPT_INFO_DUMP_BPB,
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{ 0 }
};
// *INDENT-ON*

static struct argp argp_info = {
	.options = info_options,
	.parser = parser_info,
	.args_doc = NULL,
	.doc = NULL,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL
};

error_t parser_list(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;
	struct f12_list_arguments *list_arguments = args->list_arguments;

	switch (key) {
	case (ARGP_KEY_ARG):
		if (NULL != list_arguments->path) {
			argp_usage(state);
		}
		list_arguments->path = arg;

		return 0;
	case (OPT_LIST_WITH_SIZE):
		list_arguments->with_size = 1;

		return 0;
	}

	return ARGP_ERR_UNKNOWN;
}

// *INDENT-OFF*
static struct argp_option list_options[] = {
	{
		.name = "with-size",
		.key = OPT_LIST_WITH_SIZE,
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{
		.name = "recursive",
		.key = 'r',
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{ 0 }
};
// *INDENT-ON*

static struct argp argp_list = {
	.options = list_options,
	.parser = parser_list,
	.args_doc = NULL,
	.doc = NULL,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL
};

error_t parser_move(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;
	struct f12_move_arguments *move_arguments = args->move_arguments;

	switch (key) {
	case ARGP_KEY_ARG:
		if (NULL == move_arguments->source) {
			move_arguments->source = arg;

			return 0;
		}
		if (NULL == move_arguments->destination) {
			move_arguments->destination = arg;

			return 0;
		}

		argp_usage(state);
	}

	return ARGP_ERR_UNKNOWN;
}

// *INDENT-OFF*
static struct argp_option move_options[] = {
	{
		.name = "recursive",
		.key = 'r',
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{ 0 }
};
// *INDENT-ON*

static struct argp argp_move = {
	.options = move_options,
	.parser = parser_move,
	.args_doc = NULL,
	.doc = NULL,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL
};

error_t parser_put(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;
	struct f12_put_arguments *put_arguments = args->put_arguments;

	switch (key) {
	case ARGP_KEY_ARG:
		if (NULL == put_arguments->source) {
			put_arguments->source = arg;

			return 0;
		}
		if (NULL == put_arguments->destination) {
			put_arguments->destination = arg;

			return 0;
		}

		argp_usage(state);
	}

	return ARGP_ERR_UNKNOWN;
}

// *INDENT-OFF*
static struct argp_option put_options[] = {
	{
		.name = "recursive",
		.key = 'r',
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = 0
	},
	{ 0 }
};
// *INDENT-ON*

static struct argp argp_put = {
	.options = put_options,
	.parser = parser_put,
	.args_doc = NULL,
	.doc = NULL,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL
};

// *INDENT-OFF*
static struct argp_child children[] = {
	{
		.argp = &argp_put,
		.flags = 0,
		.header = "put DEVICE SOURCE DESTINATION [OPTION...]",
		.group = 5
	},
	{
		.argp = &argp_move,
		.flags = 0,
		.header = "move DEVICE SOURCE DESTINATION [OPTION]",
		.group = 5
	},
	{
		.argp = &argp_list,
		.flags = 0,
		.header = "list DEVICE [PATH] [OPTION...]",
		.group = 5
	},
	{
		.argp = &argp_info,
		.flags = 0,
		.header = "info DEVICE [OPTION...]",
		.group = 5
	},
	{
		.argp = &argp_get,
		.flags = 0,
		.header = "get DEVICE PATH DESTINATION",
		.group = 5
	},
	{
		.argp = &argp_del,
		.flags = 0,
		.header = "del DEVICE PATH [OPTION...]",
		.group = 5
	},
	{
		.argp = &argp_create,
		.flags = 0,
		.header = "create DEVICE [ROOT_DIR] [OPTION...]",
		.group = 5
	},
	{ 0 }
};
// *INDENT-ON*

static char doc[] = "This is the documentation\n"
	"The following options are available: \v"
	"This comes after the documentation";

static char args_doc[] = "COMMAND";

// *INDENT-OFF*
static struct argp_option options[] = {
	{
		.name = NULL,
		.key = 0,
		.arg = NULL,
		.flags = 0,
		.doc = "General options:",
		.group = -3
	},
	{
		.name = "recursive",
		.key = 'r',
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = -3
	},
	{
		.name = "verbose",
		.key = 'v',
		.arg = NULL,
		.flags = 0,
		.doc = NULL,
		.group = -3
	},
	{ 0 }
};
// *INDENT-ON*

int parse_key_arg(char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (arguments->command) {
	case COMMAND_CREATE:
		return parser_create(ARGP_KEY_ARG, arg, state);
	case COMMAND_DEL:
		return parser_del(ARGP_KEY_ARG, arg, state);
	case COMMAND_GET:
		return parser_get(ARGP_KEY_ARG, arg, state);
	case COMMAND_INFO:
		return parser_info(ARGP_KEY_ARG, arg, state);
	case COMMAND_LIST:
		return parser_list(ARGP_KEY_ARG, arg, state);
	case COMMAND_MOVE:
		return parser_move(ARGP_KEY_ARG, arg, state);
	case COMMAND_PUT:
		return parser_put(ARGP_KEY_ARG, arg, state);
	default:
		return ARGP_ERR_UNKNOWN;
	}
}

void validate_arguments(struct argp_state *state)
{
	struct arguments *arguments = state->input;

	if (NULL == arguments->device_path) {
		argp_usage(state);
	}

	switch (arguments->command) {
	case COMMAND_NONE:
		argp_usage(state);
		break;
	case COMMAND_DEL:
		if (NULL == arguments->del_arguments->path) {
			argp_usage(state);
		}
		break;
	case COMMAND_GET:
		if (NULL == arguments->get_arguments->path ||
		    NULL == arguments->get_arguments->dest) {
			argp_usage(state);
		}
		break;
	case COMMAND_MOVE:
		if (NULL == arguments->move_arguments->source ||
		    NULL == arguments->move_arguments->destination) {
			argp_usage(state);
		}
		break;
	case COMMAND_PUT:
		if (NULL == arguments->put_arguments->source ||
		    NULL == arguments->put_arguments->destination) {
			argp_usage(state);
		}
		break;
	default:
		break;
	}
}

error_t parser(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case ARGP_KEY_INIT:
		arguments->recursive = 0;
		arguments->command = COMMAND_NONE;
		arguments->device_path = 0;
		for (int i = 0; i < NUMBER_OF_COMMANDS; i++) {
			state->child_inputs[i] = arguments;
		}
		break;
	case 'r':
		arguments->recursive = 1;
		break;
	case 'v':
		arguments->verbose = 1;
		break;
	case ARGP_KEY_ARG:
		if (COMMAND_NONE != arguments->command) {
			if (NULL == arguments->device_path) {
				arguments->device_path = arg;

				return 0;
			}

			return parse_key_arg(arg, state);
		}

		if (0 == strncmp(arg, "create", 7)) {
			arguments->command = COMMAND_CREATE;
		} else if (0 == strncmp(arg, "del", 4)) {
			arguments->command = COMMAND_DEL;
		} else if (0 == strncmp(arg, "get", 4)) {
			arguments->command = COMMAND_GET;
		} else if (0 == strncmp(arg, "info", 5)) {
			arguments->command = COMMAND_INFO;
		} else if (0 == strncmp(arg, "list", 5)) {
			arguments->command = COMMAND_LIST;
		} else if (0 == strncmp(arg, "move", 5)) {
			arguments->command = COMMAND_MOVE;
		} else if (0 == strncmp(arg, "put", 4)) {
			arguments->command = COMMAND_PUT;
		} else {
			argp_usage(state);
		}
		break;
	case ARGP_KEY_END:
		validate_arguments(state);
		break;
	}

	return 0;
};

static struct argp argp = {
	.options = options,
	.parser = parser,
	.args_doc = args_doc,
	.doc = doc,
	.children = children,
	.help_filter = NULL,
	.argp_domain = NULL
};

int main(int argc, char *argv[])
{
	struct arguments arguments = { 0 };
	struct f12_create_arguments create_arguments = { 0 };
	struct f12_del_arguments del_arguments = { 0 };
	struct f12_get_arguments get_arguments = { 0 };
	struct f12_info_arguments info_arguments = { 0 };
	struct f12_list_arguments list_arguments = { 0 };
	struct f12_move_arguments move_arguments = { 0 };
	struct f12_put_arguments put_arguments = { 0 };

	arguments.create_arguments = &create_arguments;
	arguments.del_arguments = &del_arguments;
	arguments.get_arguments = &get_arguments;
	arguments.info_arguments = &info_arguments;
	arguments.list_arguments = &list_arguments;
	arguments.move_arguments = &move_arguments;
	arguments.put_arguments = &put_arguments;

	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	char *output = NULL;
	int res = 0;

	switch (arguments.command) {
	case COMMAND_CREATE:
		create_arguments.device_path = arguments.device_path;
		res = f12_create(&create_arguments, &output);
		break;
	case COMMAND_DEL:
		del_arguments.device_path = arguments.device_path;
		del_arguments.recursive = arguments.recursive;
		del_arguments.verbose = arguments.verbose;
		res = f12_del(&del_arguments, &output);
		break;
	case COMMAND_GET:
		get_arguments.device_path = arguments.device_path;
		get_arguments.recursive = arguments.recursive;
		get_arguments.verbose = arguments.verbose;
		res = f12_get(&get_arguments, &output);
		break;
	case COMMAND_INFO:
		info_arguments.device_path = arguments.device_path;
		res = f12_info(&info_arguments, &output);
		break;
	case COMMAND_LIST:
		list_arguments.device_path = arguments.device_path;
		list_arguments.recursive = arguments.recursive;
		res = f12_list(&list_arguments, &output);
		break;
	case COMMAND_MOVE:
		move_arguments.device_path = arguments.device_path;
		move_arguments.recursive = arguments.recursive;
		move_arguments.verbose = arguments.verbose;
		res = f12_move(&move_arguments, &output);
		break;
	case COMMAND_PUT:
		put_arguments.device_path = arguments.device_path;
		put_arguments.recursive = arguments.recursive;
		put_arguments.verbose = arguments.verbose;
		res = f12_put(&put_arguments, &output);
		break;
	default:
		break;
	}
	if (NULL != output) {
		if (0 != res) {
			fprintf(stderr, output);
		} else {
			fprintf(stdout, output);
		}

		free(output);
	}

	return res;
}
