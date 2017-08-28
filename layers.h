// Layer record structure
#define AUTHORS_MAX 100

typedef struct layer_struct {
    bson_oid_t oid;
    char *title;
    char *description;
    enum LAYER_TYPE type;
    enum ACCESS_LAYER access;
    char *authors[AUTHORS_MAX];
} layer_s;

typedef struct layer_search_struct {
  char *id;
  char *author;
} layer_search_s;
