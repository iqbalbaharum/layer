#pragma link "/usr/local/lib/libmongoc-1.0.so"
#pragma link "/usr/local/lib/libbson-1.0.so"
#pragma include "/usr/local/include/libmongoc-1.0"
#pragma include "/usr/local/include/libbson-1.0"

#include "bcon.h"
#include "mongoc.h"
#include "bson.h"

#include "gwan.h"
#include "string.h"

#include "layert.h"
#include "layers.h"

// Prototype
void insert(xbuf_t *reply, layer_s *layer, mongoc_collection_t *collection);
void get(xbuf_t *reply, layer_search_s *search, mongoc_collection_t *collection);
void update(xbuf_t *reply, layer_s *layer, mongoc_collection_t *collection);
bool delete(xbuf_t *reply, layer_s *layer, mongoc_collection_t *collection);

// Main execution
int main (int   argc, char *argv[])
{
    mongoc_client_t *client;
    mongoc_collection_t  *collection;

    xbuf_t *reply = get_reply(argv);

   /*
    * Required to initialize libmongoc's internals
    */
    mongoc_init ();

    client = mongoc_client_new ("mongodb://localhost:27017/?appname=rivil");
    collection = mongoc_client_get_collection (client, "rivil", "layer");

   /*
    * Get HTTP_HEADERS - /GET /POST /PUT /DELETE
    */
    http_t *http = (void*)get_env(argv, HTTP_HEADERS);

    switch(http->h_method) {
      case HTTP_GET: // read record
      {
          xbuf_xcat(reply, "%s", "GET");
          layer_search_s *search = (layer_search_s *) malloc(sizeof(layer_search_s));

          get_arg("author=", &search->author, argc, argv);
          get_arg("id=", &search->id, argc, argv);

          if(search->author == NULL) {
            return HTTP_400_BAD_REQUEST;
          }

          xbuf_xcat(reply, "%s", search->author);

          get(reply, search, collection);
      }
      break;
      case HTTP_POST:
      {
          //allocate memory
          layer_s *layer = (layer_s*) malloc(sizeof(layer_s));

          // // get parameters
          char *type = 0, *access = 0, *author = 0;
          get_arg("title=", &layer->title, argc, argv);
          get_arg("description=", &layer->description, argc, argv);
          get_arg("type=", &type, argc, argv);
          get_arg("access=", &access, argc, argv);
          get_arg("author=", &author, argc, argv);

          // if compulsory component is missing, return bad request
          if(layer->title == NULL || layer->description == NULL) {
              return HTTP_400_BAD_REQUEST;
          }

          // check for type
          if(strcmp(type, BASE) == 0) {
            layer->type = BASE_LAYER;
          }
          else if(strcmp(type, DYNAMIC) == 0) {
            layer->type = DYNAMIC_LAYER;
          }
          else if (strcmp(type, DATA) == 0) {
            layer->type = DATA_LAYER;
          }
          else {
            return HTTP_400_BAD_REQUEST;
          }

          // access
          if(access && strcmp(access, PRIVATE) == 0) {
            layer->access = AC_PRIVATE;
          }
          else {
            layer->access = AC_PUBLIC;
          }


          // author
          if(author) {
            layer->authors[0] = author;
          }

          // xbuf_xcat(reply, "Hello %u", layer->type);

          insert(reply, layer, collection);
      }
      break;
      case HTTP_PUT:
      {
          //allocate memory
          layer_s *layer = (layer_s*) malloc(sizeof(layer_s));

          // // get parameters
          char *id = 0, *type = 0, *access = 0, *author = 0;
          get_arg("id=", &id, argc, argv);
          get_arg("title=", &layer->title, argc, argv);
          get_arg("description=", &layer->description, argc, argv);
          get_arg("type=", &type, argc, argv);
          get_arg("access=", &access, argc, argv);
          get_arg("author=", &author, argc, argv);

          if(id) {
              bson_oid_init_from_string (&layer->oid, id);
          } else {
              return HTTP_400_BAD_REQUEST;
          }

          if(type) {
              // check for type
              if(strcmp(type, BASE) == 0) {
                layer->type = BASE_LAYER;
              }
              else if(strcmp(type, DYNAMIC) == 0) {
                layer->type = DYNAMIC_LAYER;
              }
              else if (strcmp(type, DATA) == 0) {
                layer->type = DATA_LAYER;
              }
              else {
                return HTTP_400_BAD_REQUEST;
              }
          }

          // access
          if(access && strcmp(access, PRIVATE) == 0) {
            layer->access = AC_PRIVATE;
          }
          else {
            layer->access = AC_PUBLIC;
          }

          // author
          if(author) {
            layer->authors[0] = author;
          }

          update(reply, layer, collection);
      }
      break;
      case HTTP_DELETE: {

        //allocate memory
        layer_s *layer = (layer_s*) malloc(sizeof(layer_s));

        // // get parameters
        char *id = 0;
        get_arg("id=", &id, argc, argv);

        if(id) {

            bson_oid_init_from_string(&layer->oid, id);
            if(delete(reply, layer, collection)) {
                return HTTP_200_OK;
            }
        }

        // return HTTP_400_BAD_REQUEST;
      }
      break;
    }

    mongoc_collection_destroy (collection);
    mongoc_client_destroy (client);
    mongoc_cleanup();

   return HTTP_200_OK;
}

/***********************************************************************
 * get layer record based on author
 ***********************************************************************/
void get(xbuf_t *reply, layer_search_s *search, mongoc_collection_t *collection) {

    bson_t *pQuery = NULL;
    const bson_t *pLayer = NULL;

    xbuf_t xbuf;
    xbuf_init(&xbuf);

    // bson_t *query = BCON_NEW ("authors", BCON_UTF8(search->author));
    pQuery = bson_new();

    if(search->author) {
         BSON_APPEND_UTF8(pQuery, "authors", search->author);
    }

    // if(search->id) {
    //      BSON_APPEND_UTF8(pQuery, "id", search->id);
    // }

    jsn_t *pDocument = jsn_add_node(0, "");
    jsn_t *pResults = jsn_add_array(pDocument, "layers", 0);

    mongoc_cursor_t *pCursor = mongoc_collection_find_with_opts (collection, pQuery, NULL, NULL);

    if(pResults) {

          jsn_t *pNode;
          bson_iter_t iter;

          while(mongoc_cursor_next (pCursor, &pLayer)){
               pNode = jsn_add_node(pResults, "");

               if(bson_iter_init(&iter, pLayer)) {
                    while(bson_iter_next(&iter)) {
                         if(strcmp("_id", bson_iter_key(&iter)) == 0) {
                              bson_oid_t oid;
                              char oidstr[25];

                              if(BSON_ITER_HOLDS_OID(&iter)) {
                                   bson_oid_copy (bson_iter_oid (&iter), &oid);
                                   bson_oid_to_string(&oid, oidstr);
                                   jsn_add_string(pNode, "id", oidstr);
                              }
                         } else if (strcmp("title", bson_iter_key (&iter)) == 0) {
                              jsn_add_string(pNode, "title", bson_iter_utf8(&iter, NULL));
                         } else if (strcmp("description", bson_iter_key (&iter)) == 0) {
                              jsn_add_string(pNode, "description", bson_iter_utf8(&iter, NULL));
                         } else if (strcmp("access", bson_iter_key (&iter)) == 0) {
                              jsn_add_string(pNode, "access", bson_iter_utf8(&iter, NULL));
                         }
                    }
               }
          }
    }

    char *pText = jsn_totext(&xbuf, pDocument, 0);
    xbuf_xcat(reply, "%s", pText);

    jsn_free(pResults);
    jsn_free(pDocument);
    bson_destroy(pQuery);

}

/***********************************************************************
 * insert new layer record
 ***********************************************************************/
void insert(xbuf_t *reply, layer_s *layer, mongoc_collection_t *collection) {

    bson_t *document = bson_new();

    // id
    bson_oid_init (&layer->oid, NULL);
    // built json
    BSON_APPEND_OID (document, "_id", &layer->oid);
    BSON_APPEND_UTF8 (document, "title", layer->title);
    BSON_APPEND_UTF8 (document, "description", layer->description);

    switch(layer->type) {
      case BASE_LAYER:
        BSON_APPEND_UTF8 (document, "type", BASE);
      break;
      case DYNAMIC_LAYER:
        BSON_APPEND_UTF8 (document, "type", DYNAMIC);
      break;
      case DATA_LAYER:
        BSON_APPEND_UTF8 (document, "type", DATA);
      break;
    }

    switch(layer->access) {
      case AC_PRIVATE:
        BSON_APPEND_UTF8 (document, "access", PRIVATE);
      break;
      case AC_PUBLIC:
        BSON_APPEND_UTF8 (document, "access", PUBLIC);
      break;
    }

    // timestamp
    time_t rawtime;
    time(&rawtime);
    struct tm *created = localtime(&rawtime);
    BSON_APPEND_DATE_TIME (document, "created", mktime (created) * 1000);

    if(layer->authors[0]) {
      bson_t child;
      BSON_APPEND_ARRAY_BEGIN (document, "authors", &child);
      // in array KEY is voided
      const char *key;
      char buf[16];
      bson_uint32_to_string (0, &key, buf, sizeof buf);
      bson_append_utf8 (&child, key, -1, layer->authors[0], -1);
      bson_append_array_end (document, &child);
    }

    bson_error_t error;

    if (!mongoc_collection_insert (collection, MONGOC_INSERT_NONE, document, NULL, &error)) {
       xbuf_xcat(reply, "API Error : %s", error.message);
    } else {

      char *json = bson_as_json(document, NULL);
      // return complete json
      xbuf_xcat(reply, json, sizeof(json) - 1);
      bson_free(json);
    }

    bson_destroy(document);
}

/***********************************************************************
 * update layer record
 ***********************************************************************/
void update(xbuf_t *reply, layer_s *layer, mongoc_collection_t *collection) {

    bson_t *query = NULL;
    bson_t *update = NULL;

    query = BCON_NEW ("_id", BCON_OID(&layer->oid));
    update = bson_new();

    // update using $set
    if(layer->title || layer->description || layer->type || layer->access) {

        // xbuf_xcat(reply, "%s", layer->title);

        bson_t child;
        BSON_APPEND_DOCUMENT_BEGIN(update, "$set", &child);

        if(layer->title) {
            BSON_APPEND_UTF8 (&child, "title", layer->title);
        }

        if(layer->description) {
            BSON_APPEND_UTF8 (&child, "description", layer->description);
        }

        // type
        if(layer->type) {
            switch(layer->type) {
              case BASE_LAYER:
                BSON_APPEND_UTF8 (&child, "type", BASE);
              break;
              case DYNAMIC_LAYER:
                BSON_APPEND_UTF8 (&child, "type", DYNAMIC);
              break;
              case DATA_LAYER:
                BSON_APPEND_UTF8 (&child, "type", DATA);
              break;
            }
        }

        // change access
        if(layer->access) {
            switch(layer->access) {
              case AC_PRIVATE:
                BSON_APPEND_UTF8 (&child, "access", PRIVATE);
              break;
              case AC_PUBLIC:
                BSON_APPEND_UTF8 (&child, "access", PUBLIC);
              break;
            }
        }

        bson_append_document_end(update, &child);
    }

    // update using $addToSet
    if(layer->authors[0]) {
        bson_t authors;
        BSON_APPEND_DOCUMENT_BEGIN(update, "$addToSet", &authors);
        BSON_APPEND_UTF8 (&authors, "authors", layer->authors[0]);
        bson_append_document_end(update, &authors);
    }

    bson_error_t error;

    if (!mongoc_collection_update (
          collection, MONGOC_UPDATE_NONE, query, update, NULL, &error)) {
        xbuf_xcat(reply, "API ERROR : %s", error.message);
   }
   else {
       char *json = bson_as_json(update, NULL);
       // return complete json
       xbuf_xcat(reply, json, sizeof(json) - 1);
       bson_free(json);
   }

   bson_destroy(query);
   bson_destroy(update);
}

/***********************************************************************
 * delete specific record in DB
 ***********************************************************************/
bool delete(xbuf_t *reply, layer_s *layer, mongoc_collection_t *collection) {

    bson_t *doc;

    doc = bson_new();
    BSON_APPEND_OID(doc, "_id", &layer->oid);
    xbuf_xcat(reply, "Failed with API error : %s", layer->oid);

    bson_error_t error;
    if (!mongoc_collection_remove (
          collection, MONGOC_REMOVE_SINGLE_REMOVE, doc, NULL, &error)) {
      xbuf_xcat(reply, "Failed with API error : %s", error.message);
      return false;
   }

   return true;
}
