#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include "f12.h"

#define COMMAND_NONE 0
#define COMMAND_CREATE 1
#define COMMAND_DEFRAG 2
#define COMMAND_DEL 3
#define COMMAND_GET 4
#define COMMAND_INFO 5
#define COMMAND_LIST 6
#define COMMAND_MOVE 7
#define COMMAND_PUT 8
#define COMMAND_RESIZE 9

#define OPT_CREATE_SECTOR_SIZE 1
#define OPT_CREATE_SECTORS_PER_CLUSTER 2
#define OPT_CREATE_VOLUME_NAME 3
#define OPT_CREATE_VOLUME_SIZE 4
#define OPT_DEFRAG_SIMULATE 5
#define OPT_DEL_SOFT_DELETE 6
#define OPT_INFO_DUMP_BPB 7
#define OPT_LIST_WITH_SIZE 8
#define OPT_RESIZE_SIZE 9

const char *argp_program_version = "0.0";
const char *argp_program_bug_address = "Karsten Lehmann <mail@kalehmann.de>";

struct arguments
{
  struct f12_create_arguments *create_arguments;
  struct f12_defrag_arguments *defrag_arguments;
  struct f12_del_arguments *del_arguments;
  struct f12_get_arguments *get_arguments;
  struct f12_info_arguments *info_arguments;
  struct f12_list_arguments *list_arguments;
  struct f12_move_arguments *move_arguments;
  struct f12_put_arguments *put_arguments;
  struct f12_resize_arguments *resize_arguments;
  char *device_path;
  int no_interaction;
  int recursive;
  int command;
};

error_t parser_create(int key, char *arg, struct argp_state *state)
{
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option create_options[] = {
  {"volume-size", OPT_CREATE_VOLUME_SIZE, "SIZE"},
  {"sector-size", OPT_CREATE_SECTOR_SIZE, "SIZE"},
  {"sectors-per-cluser", OPT_CREATE_SECTORS_PER_CLUSTER, "N"},
  {"volume-name", OPT_CREATE_VOLUME_NAME, "NAME"},
  {0}
};

static struct argp argp_create = {create_options, parser_create};

error_t parser_defrag(int key, char *arg, struct argp_state *state)
{
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option defrag_options[] = {
  {"simulate", OPT_DEFRAG_SIMULATE},
  {0}
};

static struct argp argp_defrag = {defrag_options, parser_defrag};

error_t parser_del(int key, char *arg, struct argp_state *state)
{
  struct arguments *args = state->input;
  struct f12_del_arguments *del_arguments = args->del_arguments;

  switch (key)
    {
    case (OPT_DEL_SOFT_DELETE):
      del_arguments->soft_delete = 1;
      return 0;
    case (ARGP_KEY_ARG):
      if (del_arguments->path != 0 && del_arguments->path[0] != '\0')
	argp_usage(state);
      del_arguments->path = arg;
      return 0;
    }
  
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option del_options[] = {
  {"recursive", 'r'},
  {"soft-delete", OPT_DEL_SOFT_DELETE},
  {0}
};

static struct argp argp_del = {del_options, parser_del};

error_t parser_get(int key, char *arg, struct argp_state *state)
{
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option get_options[] = {
  {"recursive", 'r'},
  {0}
};

static struct argp argp_get = {get_options, parser_get};  

error_t parser_info(int key, char *arg, struct argp_state *state)
{
  struct arguments *args = state->input;
  struct f12_info_arguments *info_arguments = args->info_arguments;
  
  switch (key)
    {
    case (OPT_INFO_DUMP_BPB):
      info_arguments->dump_bpb = 1;
      return 0;
    }
  
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option info_options[] = {
  {"dump-bpb", OPT_INFO_DUMP_BPB},
  {0}
};

static struct argp argp_info = {info_options, parser_info};

error_t parser_list(int key, char *arg, struct argp_state *state)
{
  struct arguments *args = state->input;
  struct f12_list_arguments *list_arguments = args->list_arguments;

  switch (key)
    {
    case (ARGP_KEY_ARG):
      if (list_arguments->path != 0 && list_arguments->path[0] != '\0')
	argp_usage(state);
      list_arguments->path = arg;
      return 0;
    case (OPT_LIST_WITH_SIZE):
      list_arguments->with_size = 1;
      return 0;
    }
  
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option list_options[] = {
  {"with-size", OPT_LIST_WITH_SIZE},
  {"recursive", 'r'},
  {0}
};

static struct argp argp_list = {list_options, parser_list};

error_t parser_move(int key, char*arg, struct argp_state *state)
{  
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option move_options[] = {
  {"recursive", 'r'},
  {0}
};

static struct argp argp_move = {move_options, parser_move};

error_t parser_put(int key, char *arg, struct argp_state *state)
{
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option put_options[] = {
  {"recursive", 'r'},
  {0}
};
static struct argp argp_put = {put_options, parser_put};

error_t parser_resize(int key, char *arg, struct argp_state *state)
{
  return ARGP_ERR_UNKNOWN;
}

static struct argp_option resize_options[] = {
  {"size", OPT_RESIZE_SIZE, "SIZE"},
  {0}
};

static struct argp argp_resize = {resize_options, parser_resize};

static struct argp_child children[] = {
  {&argp_create, 0, "create DEVICE [OPTION...]", 5},
  {&argp_defrag, 0, "defrag DEVICE [OPTION...]", 5},
  {&argp_del, 0, "del DEVICE PATH [OPTION...]", 5},
  {&argp_get, 0, "get DEVICE PATH [DESTINATION] [OPTION...]", 5},
  {&argp_info, 0, "info DEVICE [OPTION...]", 5},
  {&argp_list, 0, "list DEVICE [PATH] [OPTION...]", 5},
  {&argp_move, 0, "move DEVICE SOURCE DESTINATION [OPTION]", 5},
  {&argp_put, 0, "put DEVICE SOURCE DESTINATION [OPTION...]", 5},
  {&argp_resize, 0, "resize DEVICE [OPTION...]", 5},
  {0}
};

static char doc[] = "This is the documentation\nThe following options are available: \vThis comes after the documentation";

static char args_doc[] = "COMMAND";

static struct argp_option options[] = {
  {0,0,0,0,"General options:", -3},
  {"recursive", 'r',0,0,0,-3},
  {"yes", 'y',0,0,0,-3},
  {0}
};

int parse_key_arg(char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;
  
  switch (arguments->command)
  {
  case COMMAND_CREATE:
    return parser_create(ARGP_KEY_ARG, arg, state);
  case COMMAND_DEFRAG:
    return parser_defrag(ARGP_KEY_ARG, arg, state);
  case COMMAND_DEL:
    return parser_del(ARGP_KEY_ARG, arg, state);
  case COMMAND_GET:
    return parser_get(ARGP_KEY_ARG, arg, state);
  case COMMAND_INFO:
    return parser_info(ARGP_KEY_ARG, arg, state);
  case COMMAND_LIST:
    return parser_list(ARGP_KEY_ARG, arg, state);
  }
}

error_t parser(int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;
  
  switch (key)
  {
  case ARGP_KEY_INIT:
    arguments->no_interaction = 0;
    arguments->recursive = 0;
    arguments->command = COMMAND_NONE;
    arguments->device_path = 0;
    for (int i=0; i<9; i++) {
      state->child_inputs[i] = arguments;
    }
    break;
  case 'y':
    arguments->no_interaction = 1;
    break;
  case 'r':
    arguments->recursive = 1;
    break;
  case ARGP_KEY_ARG:
    if (COMMAND_NONE != arguments->command) { 
      if (0 != arguments->device_path) {
	return parse_key_arg(arg, state);
      }
      arguments->device_path = arg;

      return 0;
    }
    
    if (0 == strncmp(arg, "create", 7)) {
      arguments->command = COMMAND_CREATE;
    } else if (0 == strncmp(arg, "defrag", 7)) {
      arguments->command = COMMAND_DEFRAG;
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
    } else if (0 == strncmp(arg, "resize", 7)) {
      arguments->command = COMMAND_RESIZE;
    } else {
      argp_usage(state);
    }
    break;
  case ARGP_KEY_END:
    switch (arguments->command)
      {
      case COMMAND_NONE:
	argp_usage(state);
	break;
      case COMMAND_DEL:
	if (arguments->del_arguments->path == 0) {
	  argp_usage(state);
	}
	break;
      }
    break;
  }
  
  return 0;
};

static struct argp argp = {options,parser,args_doc,doc, children};

int main(int argc, char* argv[])
{
  struct arguments arguments = {0};
  struct f12_create_arguments create_arguments = {0};
  struct f12_defrag_arguments defrag_arguments = {0};
  struct f12_del_arguments del_arguments = {0};
  struct f12_get_arguments get_arguments = {0};
  struct f12_info_arguments info_arguments = {0};
  struct f12_list_arguments list_arguments = {0};
  struct f12_move_arguments move_arguments = {0};
  struct f12_put_arguments put_arguments = {0};
  struct f12_resize_arguments resize_arguments = {0};
  
  arguments.create_arguments = &create_arguments;
  arguments.defrag_arguments = &defrag_arguments;
  arguments.del_arguments = &del_arguments;
  arguments.get_arguments = &get_arguments;
  arguments.info_arguments = &info_arguments;
  arguments.list_arguments = &list_arguments;
  arguments.move_arguments = &move_arguments;
  arguments.put_arguments = &put_arguments;
  arguments.resize_arguments = &resize_arguments;

  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  char *output = NULL;
  
  switch (arguments.command)
    {
    case COMMAND_CREATE:
      
      break;
    case COMMAND_DEFRAG:

      break;
    case COMMAND_DEL:
      arguments.del_arguments->device_path = arguments.device_path;
      arguments.del_arguments->recursive = arguments.recursive;
      switch(f12_del(arguments.del_arguments, &output))
	{
	case 0:
	  printf(output);
	  break;
	}
      break;
    case COMMAND_GET:
      
      break;
    case COMMAND_INFO:
      arguments.info_arguments->device_path = arguments.device_path;
      switch (f12_info(arguments.info_arguments, &output))
	{
	case -1:
	  printf("Error while reading the file\n");
	  break;
	case 0:
	  printf(output);
	  break;
	default:
	  printf("Error\n");
	  break;
	}
      break;
    case COMMAND_LIST:
      arguments.list_arguments->device_path = arguments.device_path;
      arguments.list_arguments->recursive = arguments.recursive;
      switch (f12_list(arguments.list_arguments, &output))
	{
	case 0:
	  printf(output);
	  break;
	}
      break;
    case COMMAND_MOVE:

      break;
    case COMMAND_RESIZE:

      break;
    }
  free(output);
  
  exit (0);
}
