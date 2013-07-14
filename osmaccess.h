#include <stdlib.h>
#include <stdio.h>

#include "./osmformat.pb-c.h"
#include "./fileformat.pb-c.h"


struct Parser {
  uint8_t* data;
  uint8_t* cur;
  size_t len;
};

typedef struct Blob {
  OSMPBF__BlobHeader* bh;
  OSMPBF__Blob* b;
  struct Blob* next;
  uint8_t* dref; // reference to starting point of header (ie. header lenght field)
} Blob;


Blob* init_Blob(Blob* b, OSMPBF__BlobHeader* bh, OSMPBF__Blob* bb, struct Blob* n, uint8_t* dref);

size_t readFile(uint8_t** data, char* fname);

int fprint_info(FILE* stream, OSMPBF__Info* info);
int fprint_blob_summary(FILE* stream, Blob* b);

struct Blob* parse_all_blobs(struct Parser* const p);
void unpack_osmblob(Blob* blob);


///////////  Element Filter  ///////////////////////////////////////////






/** key value condition */
typedef struct kvCondition {
  char* key;
  char* val;
} kvCondition;

typedef struct kvReCondition {
  regex_t* key;
  regex_t* val;
} kvReCondition;

enum typeCondition {node, way, relation};

enum optype {OPand, OPor, OPnot, CondKV, CondKVre, CondType, NOP, END };

/** operation or operand for filtering osm elements.
 */
typedef struct filterop {
  enum optype type;
  // Conditions, only if type is not OP*
  union { 
    kvCondition kv_cond;
    kvReCondition kv_re_cond;
    enum typeCondition type_cond;
  };
} filterop;



typedef struct filter {
  int* stack;
  int* sp;

  filterop* cond;
  filterop* cp;

} filter;

filter* newFilterInit(filterop* fa);

/** Find all elements that adhere to the conditions in the filter description fa.
 * @param fa condition for elements, in reverse polish notation
 * @param opnum number of operations in array
 * @param handler handling function to deal with the found elements
 * @param data handed over to handler
 */
void findAllElements_ar(filterop* fa, void (*handler)(void* data, void* element), void* data);

