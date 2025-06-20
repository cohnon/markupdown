#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

//= Markdown -> HTML Renderer =//
#define CUR (**it)
#define NEXT (*(*it + 1))
#define NEXT2 (*(*it + 2))
#define INC(N) (*it += (N))
#define DEC(N) (*it -= (N))

static void skip_whitespace(char **it) {
  while (CUR == ' ' || CUR == '\t' || CUR == '\n') {
    INC(1);
  }
}

static void render_text(char **it, FILE *out);

static void render_bold(char **it, FILE *out) {
  INC(1); // *
  fprintf(out, "<b>");

  while (CUR != '*') {
    fputc(CUR, out);
    INC(1);
  }

  INC(1); // *
  fprintf(out, "</b>");
}

static void render_code(char **it, FILE *out) {
  INC(1); // `
  fprintf(out, "<code>");

  while (CUR != '`') {
    fputc(CUR, out);
    INC(1);
  }

  INC(1); // `
  fprintf(out, "</code>");
}

void render_link(char **it, FILE *out) {
  INC(1); // [
  char text[128];
  long text_len = 0;
  while (CUR != ']') {
    text[text_len++] = CUR;
    INC(1);
  }

  text[text_len] = '\0';

  if (NEXT != '(') {
    fprintf(stderr, "expected '(' after link text\n");
    exit(EXIT_FAILURE);
  }

  INC(2); // ](

  fprintf(out, "<a href=\"");

  while (CUR != ')') {
    fputc(CUR, out);
    INC(1);
  }

  INC(1); // )

  fprintf(out, "\">%s</a>", text);
}

static void render_text(char **it, FILE *out) {
  while (CUR != '\n') {
    switch (CUR) {
    case '*':
      render_bold(it, out);
      break;

    case '`':
      render_code(it, out);
      break;

    case '[':
      render_link(it, out);
      break;

    default:
      fputc(CUR, out);
      INC(1);
    }
  }

  INC(1); // \n
}

// blocks
static void render_header(char **it, FILE *out) {
  int level = 0;
  while (CUR == '#') {
    level += 1;
    INC(1);
  }

  skip_whitespace(it);

  fprintf(out, "<h%d>", level);

  while (CUR != '\n') {
    fputc(CUR, out);
    INC(1);
  }

  fprintf(out, "</h%d>\n", level);
}

static void render_list_item(char **it, FILE *out) {
  fprintf(out, "<li>");

  skip_whitespace(it);

  render_text(it, out);

  fprintf(out, "</li>\n");
}

static void render_unordered_list(char **it, FILE *out) {
  fprintf(out, "<ul>\n");

  while (CUR == '-') {
    INC(1); // -
    render_list_item(it, out);
  }

  fprintf(out, "</ul>\n");
}

static void render_ordered_list(char **it, FILE *out) {
  fprintf(out, "<ol>\n");

  while ('0' <= CUR && CUR <= '9') {
    if (NEXT != '.') {
      break;
    }
    INC(2); // N.

    render_list_item(it, out);
  }

  fprintf(out, "</ol>\n");
}

static bool is_code_block_delimeter(char **it) {
  return CUR == '`' && NEXT == '`' && NEXT2 == '`';
}

static void render_code_block(char **it, FILE *out) {
  INC(3);
  fprintf(out, "<pre><code>");

  while (!is_code_block_delimeter(it)) {
    fputc(CUR, out);
  }

  INC(3);
  fprintf(out, "</pre></code>");
}

static void render_text_block(char **it, FILE *out) {
  fprintf(out, "<p>");

  while (CUR != '\n' && CUR != '\0') {
    render_text(it, out);
    fputc(' ', out);
  }

  if (CUR == '\n') {
    INC(1); // \n
  }

  fprintf(out, "</p>\n");
}

// render markdown into html and append it to an out file
static void render_markdown(char *markdown, FILE *out) {
  char *it = markdown;

  skip_whitespace(&it);
  while (*it != '\0') {
    switch (*it) {
    case '#':
      render_header(&it, out);
      break;

    case '-':
      render_unordered_list(&it, out);
      break;

    case '1':
      if (*(it + 1) == '.') {
        render_ordered_list(&it, out);
      } else {
        render_text_block(&it, out);
      }
      break;

    case '`':
      if (is_code_block_delimeter(&it)) {
        render_code_block(&it, out);
      } else {
        render_text_block(&it, out);
      }
      break;

    default:
      render_text_block(&it, out);
      break;
    }

    skip_whitespace(&it);
  }
}
//= Markdown -> HTML Renderer =//

//= Markupdown =//
#define TEMPLATE_FILE_NAME ".template.html"
#define TEMPLATE_CONTENT_COMMENT "<!-- content -->\n"

char *template = NULL;
long template_len = 0;
long template_content_offset = 0;

char *src_path = NULL;
char *dst_path = NULL;

static long get_file_size(FILE *file) {
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  return size;
}

static char *read_file(FILE *file) {
  long file_size = get_file_size(file);
  char *content = malloc(file_size + 1);
  fread(content, sizeof(char), file_size, file);
  content[file_size] = '\0';

  return content;
}

static bool is_file(char *path) {
  struct stat file_stat;
  stat(path, &file_stat);
  return S_ISREG(file_stat.st_mode);
}

static bool is_directory(char *path) {
  struct stat file_stat;
  stat(path, &file_stat);
  return S_ISDIR(file_stat.st_mode);
}

static bool is_markdown(char *path) {
  long path_len = strlen(path);
  if (path_len < 4) {
    printf("cannot convert %s to .html\n", path);
    exit(EXIT_FAILURE);
  }

  return strncmp(&path[path_len - 3], ".md", sizeof(".md")) == 0;
}

static void markdown_to_html(char *path, char *html_path) {
  char *relative_path = path + strlen(src_path);

  strcpy(html_path, dst_path);
  strncat(html_path, relative_path, strlen(relative_path) - 2);
  strcat(html_path, "html");
}

static void load_template(void) {
  unsigned long src_len = strlen(src_path);

  unsigned long template_path_len = src_len + strlen(TEMPLATE_FILE_NAME);
  char template_path[template_path_len + 2];

  strcpy(template_path, src_path);
  strcat(template_path, "/");
  strcat(template_path, TEMPLATE_FILE_NAME);

  FILE *template_file = fopen(template_path, "rb");
  if (template_file == NULL) {
    printf("[load_template] could not open %s\n", template_path);
    exit(EXIT_FAILURE);
  }

  template_len = get_file_size(template_file);

  template = malloc(template_len + 1);
  if (template == NULL) {
    printf("[load_template] malloc failed\n");
    fclose(template_file);
    exit(EXIT_FAILURE);
  }

  template[template_len] = '\0';

  fread(template, sizeof(char), template_len, template_file);

  fclose(template_file);

  long content_comment_len = strlen(TEMPLATE_CONTENT_COMMENT);
  for (long i = 0; i < template_len - content_comment_len; i++) {
    if (strncmp(template + i, TEMPLATE_CONTENT_COMMENT, content_comment_len) ==
        0) {
      template_content_offset = i + content_comment_len;
    }
  }
}

// autogenerate index.html for each subdirectory with links.
// users can specify a custom index.md that will be rendered before the links.
//
// XXX.md -> XXX.html
// XXX/ -> XXX/index.html
static void generate(char *path) {
  if (is_directory(path)) {
    struct dirent *dir_entity;
    DIR *dir = opendir(path);

    // create dst_path/index.html
    // fill with optional src_path/index.md
    // append with links
    char src_index_path[1024];
    char dst_index_path[1024];

    strcpy(src_index_path, path);
    strcat(src_index_path, "/index.md");
    markdown_to_html(src_index_path, dst_index_path);

    int slash_pos = strlen(dst_index_path) - 11;
    dst_index_path[slash_pos] = '\0';
    mkdir(dst_index_path, 0777);
    dst_index_path[slash_pos] = '/';
    FILE *out_index = fopen(dst_index_path, "w");
    if (out_index == NULL) {
      printf("Failed to create %s\n", dst_index_path);
      exit(EXIT_FAILURE);
    }

    fprintf(out_index, "%.*s", (int)template_content_offset, template);

    FILE *index = fopen(src_index_path, "rb");
    if (index != NULL) {
      char *markdown = read_file(index);
      render_markdown(markdown, out_index);
    }

    fprintf(out_index, "<nav><ul>\n");

    for (;;) {
      dir_entity = readdir(dir);
      if (dir_entity == NULL) {
        break;
      }

      char *name = dir_entity->d_name;

      // ignore hidden files and "." ".."
      if (name[0] == '.') {
        continue;
      }

      // files must be markdown
      if (is_file(name) && !is_markdown(name)) {
        continue;
      }

      // ignore index.md
      if (strcmp(name, "index.md") == 0) {
        continue;
      }

      if (is_file(name)) {
        int name_len = strlen(name) - 3;
        fprintf(out_index, "<a href=\"./%.*s.html\">%.*s</a>\n", name_len, name,
                name_len, name);
      } else {
        fprintf(out_index, "<a href=\"./%s\">%s/</a>\n", name, name);
      }

      char subpath[1024];
      strcpy(subpath, path);
      strcat(subpath, "/");
      strcat(subpath, dir_entity->d_name);

      generate(subpath);
    }

    fprintf(out_index, "</ul></nav>\n");

    fprintf(out_index, "%s", template + template_content_offset);

  } else if (is_file(path) && is_markdown(path)) {
    FILE *in = fopen(path, "rb");
    if (in == NULL) {
      printf("[generate] could not open %s\n", path);
    }

    char html_path[1024];
    markdown_to_html(path, html_path);
    FILE *out = fopen(html_path, "w");
    if (out == NULL) {
      printf("[generate] failed to create %s\n", html_path);
    }

    fprintf(out, "%.*s", (int)template_content_offset, template);
    char *markdown = read_file(in);
    render_markdown(markdown, out);
    fprintf(out, "%s", template + template_content_offset);
    free(markdown);

    fclose(in);
    fclose(out);
  }
}

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("usage: %s <src> <dst>\n", argv[0]);
    return EXIT_FAILURE;
  }

  src_path = argv[1];
  dst_path = argv[2];

  if (!is_directory(src_path) || !is_directory(dst_path)) {
    printf("arguments must be directories\n");
    exit(EXIT_FAILURE);
  }

  load_template();
  generate(src_path);

  free(template);

  return EXIT_SUCCESS;
}
//= Markupdown =//
