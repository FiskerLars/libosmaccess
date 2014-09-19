#include "osmaccess.h"

int main(int argc, char** argv) {
  uint8_t* data = 0;
  size_t len = 0;

  len = readFile(&data, argv[1]);
  fprintf(stderr, "Read %d bytes from file %s\n", len, argv[1]);
  if( len > 0 ) {
    struct Parser p = {data, data, len};
    struct Blob* b = parse_all_blobs(&p);
    fprint_blob_summary(stdout, b);
    
    struct Blob* bp;
    for(bp = b; bp ; bp = bp->next) {
      unpack_osmblob(bp);//bp->b, bp->bh->type);
    }
      
  }
  
  
  exit(0);
  ProtobufCMessage* message =
    protobuf_c_message_unpack(&osmpbf__blob__descriptor,
			      0, len, data+4);
  
  if ( message == 0 ) {
    fprintf(stderr, "%s:%d error parsing\n", __FILE__,__LINE__);
  }
  
  //    osm_blob = osmpbf__blob_header__unpack(NULL, len, data);

  return 0;
}
