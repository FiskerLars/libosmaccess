#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
#include <string.h>

#include "./osmformat.pb-c.h"
#include "./fileformat.pb-c.h"

/* 
-I ~/src/OSM-binary/include/ -L /home/tiga/src/OSM-binary/src/ 
*/
// dirty c++: #include <osmpbf/osmpbf.h>

// -lpthread -lz -lprotobuf-lite -losmpbf



#define CHUNKSIZE (1024 * sizeof(uint8_t))
size_t readFile(uint8_t** data, char* fname) {
  int fd = open(fname, O_RDONLY);
  struct stat fs; 
  size_t len = 0;
  uint8_t* d = 0;
  if(fd >= 0) {
    fstat(fd, &fs);
    d = malloc(fs.st_size);
    len = read(fd, d, fs.st_size);
    close(fd);
    *data = d;
  }
  return len;
}

/** Print data to stream in hexdump style.
 */
int fhexprint(FILE* stream, uint8_t* data, size_t len) {
  size_t n = 0; // cur position in data 
  size_t lwidth = 16;
  // size_t lstep = 2;
  uint8_t* p = data;
    for(; n < len; n += lwidth ) {
    fprintf(stream, "%08x", n);
    for(int i = 0; i < lwidth && n+i < len; i++) {
      fprintf(stream, " %02x", (unsigned int)p+i);
    }
    fprintf(stream, "\n");
    p += lwidth;
  }
  
  //  fwrite(p, 1, n, stream);
  return n;
}

/** Unpack some zlib deflated data. Will allocate sufficient space and store it in dst.
 * TODO: inflate only returns zeros...
 */
long unpack_zlib_data(uint8_t* dst, unsigned long dst_size, uint8_t* const data, size_t const len){
  // TODO
  // ZEXTERN int ZEXPORT uncompress OF((Bytef *dest,   uLongf *destLen,
  //                                 const Bytef *source, uLong sourceLen));

  uncompress(dst, &dst_size, data, len);
  return dst_size;

  /*
  long r = 0; // lenght of data put into *dst
  uint8_t* d = malloc(1024*sizeof(uint8_t)); 
  size_t cur = 1024;
  z_stream zs;
  fprintf(stderr,  "%s:%d *data:0x%08x...\n", __FILE__, __LINE__, *(uint32_t*)data);
  zs.next_in = data;
  zs.avail_in = len;
  zs.zalloc = Z_NULL; // default
  zs.zfree = Z_NULL; // default
  zs.opaque = Z_NULL;  // private data object given to zalloc or zfree
 
  
  int r_ini = inflateInit(&zs);
  if (Z_OK == r_ini) { // everything is fine
    int r_inf = 0;
    fprintf(stderr, "%s:%d inflateInit Z_OK\n", __FILE__, __LINE__);
    
    do {
      do {
	int have = 0;
	uint8_t outchunk[1024];
	zs.next_out = outchunk;
	zs.avail_out = 1024;

	r_inf = inflate(&zs, Z_SYNC_FLUSH);

	switch (r_inf) {
	case Z_STREAM_END:
	  fprintf(stderr, "%s:%d zlib stream end\n", __FILE__, __LINE__);
	case Z_OK:
	  have = 1024 - zs.avail_out;
	  fprintf(stderr, "%s:%d avail_out %d, next_out:0x%08x, *next_out:0x%08x.., total_out:%ld, have:%d, d:0x%08x, r=%ld\n", 
		  __FILE__, __LINE__, zs.avail_out, (unsigned int)zs.next_out, *(uint32_t*)zs.next_out, zs.total_out, have, (unsigned int)d, r);
	  memcpy( d+r, zs.next_out, have);

	  r += have;
	
	  if(r_inf != Z_STREAM_END) {
	    uint8_t* tmp = realloc(d, cur+1024);
	    if(0 == tmp) 
	      fprintf(stderr, "%s:%d Failed to alloc mem\n", __FILE__, __LINE__);
	    d = tmp;
	    cur += 1024;
	  }
	  break;
	default: 
	  fprintf(stderr, "Error %d inflating\n", r_inf);
	} 
      } while (zs.avail_out == 0);
    } while (r_inf != Z_STREAM_END);
    *dst = realloc(d, r); //prune d to size r should never fail
    fprintf(stderr, "%s:%d *dst:%08x, r:%ld\n", __FILE__, __LINE__, (unsigned int)*dst, r);
    inflateEnd(&zs);
  }
  return r;
*/
}



//////////////////////////////////////////////////////////////////////////////////////////////


typedef struct Blob {
  OSMPBF__BlobHeader* bh;
  OSMPBF__Blob* b;
  struct Blob* next;
  uint8_t* dref; // reference to starting point of header (ie. header lenght field)
} Blob;


Blob* init_Blob(Blob* b, OSMPBF__BlobHeader* bh, OSMPBF__Blob* bb, struct Blob* n, uint8_t* dref) {
  b->bh = bh;
  b->b = bb;
  b->next = n;
  b->dref = dref;
  return b;
}

int fprint_info(FILE* stream, OSMPBF__Info* info){
  int n = 0;
  if(info->has_version) 
    n += fprintf(stream, "version %d\n", info->version);
  if(info->has_timestamp)
    n += fprintf(stream, "timestamp %lld\n", info->timestamp);
  if(info->has_changeset)
    n += fprintf(stream, "changeset %lld\n", info->changeset);
  if(info->has_uid)
    n += fprintf(stream, "changeset %d\n", info->uid);
  if(info->has_user_sid)
    n += fprintf(stream, "user sid %d\n", info->user_sid);
  n += fprintf(stream, "%s,%s\n", info->has_visible?"has visible":"", info->visible?"visible":"");
  return n;
}

/** Print a summary of the list of blobs in b to stream.
 */
int fprint_blob_summary(FILE* stream, Blob* b) {
  int len = 0;
  int num = 0;
  size_t dlen = 0;
  int num_raw = 0;
  int num_zlib = 0;
  int num_lzma = 0;
  Blob* i;
  // print types and data-types and sizes
  for(i = b, num=1; i != 0; num++, i = i->next) {
    OSMPBF__BlobHeader* bh = i->bh;
    OSMPBF__Blob* bb = i->b;
    len += fprintf(stream, "Blob %4d: %s %7d[bytes]\n", num, bh->type, bh->datasize);
    dlen += bh->datasize;
    if(bb) {
      if( bb->has_raw ) {
	len += fprintf(stream, "\t raw data: %d[bytes]\n", dlen);
	num_raw++;
      }
      if( bb->has_zlib_data ) {
	len += fprintf(stream, "\tzlib data, raw size: %d[bytes]\n", bb->has_raw_size?bb->raw_size:-1);
	num_zlib++;
      }
      if( bb->has_lzma_data ) {
	len += fprintf(stream, "\tlzma data\n");
	num_lzma++;
      }
    }
  }
  num--;
  fprintf(stream, "--------------------------\n");
  fprintf(stream, "%4d blobs with %d[bytes] of data\n", num, dlen);
  fprintf(stream, "%4d blobs raw data, %d blobs zlib data, %d blobs lzma data\n", num_raw, num_zlib, num_lzma);
  return len;
}

////////////////////////////////////////////////////////////////////////////////////////////

struct Parser {
  uint8_t* data;
  uint8_t* cur;
  size_t len;
};




/** Parse a BlobHeader. Allocates new OSMPBF_BlobHeader and updates p.
 */
OSMPBF__BlobHeader* parse_header(struct Parser* p) {
  uint32_t headlen = ntohl(*(uint32_t*)(p->cur));
  OSMPBF__BlobHeader* blobhead ;
  p->cur += sizeof(uint32_t);
  blobhead = osmpbf__blob_header__unpack(0, headlen, p->cur);
  if ( blobhead == 0 ) {
      fprintf(stderr, "%s:%d error parsing headerdirectly\n", __FILE__,__LINE__);
      p->cur -= sizeof(uint32_t);
  } else {
    //     fprintf(stderr, "osm_blob_header: typey[0x%08x] %s, datasize %d, has_indexdata %s\n", 
    //	      blobhead->type, blobhead->type, blobhead->datasize, (blobhead->has_indexdata!=0?"true":"false"));
      p->cur += headlen;
  }  
  return blobhead;
}


OSMPBF__Blob* parse_blob_body(struct Parser* const  p, const OSMPBF__BlobHeader* const bh) {
  OSMPBF__Blob* b = 0;
  b = osmpbf__blob__unpack(0, bh->datasize, p->cur);
  if (b == 0) 
    fprintf(stderr, "%s:%d error parsing blob of type %s\n",
	    __FILE__, __LINE__, bh->type);
  else {
    fprintf(stderr, "osm_blob: has_raw %d, has_raw_size %d, has_zlib_data %d, has_lzma_data %d\n",
	    b->has_raw, b->has_raw_size, b->has_zlib_data, b->has_lzma_data);
    p->cur += bh->datasize;
  }
  return b;
}


/** Parse a whole blob including header from the stream. Does not unpack yet.
 */
struct Blob* parse_blob(struct Parser* const p) {
  OSMPBF__BlobHeader* bh = parse_header(p);
  OSMPBF__Blob* bb = 0;
  struct Blob* b = 0;
  uint8_t* dstart = p->cur;
  if(bh) {
    bb = parse_blob_body(p, bh);
    if(bb) {
      b = malloc(sizeof(struct Blob));
      init_Blob(b, bh, bb, 0, dstart);
      //      fprintf(stderr,"init_Blob(b:%08x, bh:%08x, bb:%08x, 0, dstart:%08x\n", b, bh, bb, dstart);
    } else 
      free(bh);
  }
  return  b;
}


/** Parse data into a linked list of blobs.
 */
struct Blob* parse_all_blobs(struct Parser* const p) {
  struct Blob* b = 0;
  struct Blob* last = 0;
  b = parse_blob(p); //parse a first
  for( last=b; 
       p->cur - p->data < p->len; 
       last=last->next ) {
    last->next = parse_blob(p);
    if (last->next == 0)
      break;
  }
  return b;
}




/////////////////////////////////////////////////////////////////////////////////////////////////


char* stringtable_get_string(OSMPBF__StringTable* st, int i) {
  if(i < st->n_s) {
    char* res = malloc(sizeof(uint8_t)*((st->s[i]).len + 1));
    if(res) {
      res[st->s[i].len] = 0;
      memcpy(res, st->s[i].data, st->s[i].len);
      return res;
    }
  }
  return 0;
}

// Converting an integer to a lattitude or longitude uses the formula: OUT = IN * granularity / 10**9$
double int2deg(int32_t granularity, int64_t in) {
  return 1.0;//(in*granularity) / (double)10 ** 9;
}

/** TODO: actually do something and unpack data.
 */
void unpack_osmblob(OSMPBF__Blob* osm_blob, char* const type) {
  fprintf(stderr, "Unpacking osmblob %s\n", type);
  assert(osm_blob);
  uint8_t* zd = 0;
  long len = 0;
  int must_free = 0;
  if(osm_blob->has_raw){
    // TODO zd = (uint8_t*)osm_blob->raw;
  } else  if(osm_blob->has_lzma_data){
    // TODO
  } else

    // unpack zlib_data
    if(osm_blob->has_zlib_data){
    assert(osm_blob->has_raw_size);
    zd = malloc(osm_blob->raw_size + 1);
    must_free = 1;
    zd[osm_blob->raw_size] = 0;
    len = unpack_zlib_data(zd, osm_blob->raw_size,  osm_blob->zlib_data.data, osm_blob->zlib_data.len);
  }  
  
  // parse whatever has been contained.
  if(!strcmp("OSMHeader", type)) { 
    OSMPBF__HeaderBlock* osm_head =  osmpbf__header_block__unpack(0, len, zd);
    // Debugging stuff
    fprintf(stderr, "%s:%d OSM Head parsed:\n\tsource: %s\n\twritingprogram: %s\n\tn_required_f: %d\n\tn_optional_:%d\n",
	    __FILE__,__LINE__,
	    (osm_head->source?osm_head->source:"[no source]"), 
	    (osm_head->writingprogram?osm_head->writingprogram:"[no w-prog]"),
	    osm_head->n_required_features, 
	    osm_head->n_optional_features);
    
    if(osm_head->bbox) {
      OSMPBF__HeaderBBox* bbox = osm_head->bbox;
      fprintf(stderr, "bbox (%lld, %lld) -- (%lld, %lld)\n", bbox->left, bbox->top, bbox->right, bbox->bottom);
      // osmpbf__header_bbox__free_unpacked(bbox, 0);   
 }
    // print features
    int i;
    for(i = 0; i < osm_head->n_required_features; i++)
      fprintf(stderr, 
	      "Feature: %s\n", 
	      (osm_head->required_features)[i]?(osm_head->required_features)[i]:"[zero]");
    for(i = 0; i < osm_head->n_optional_features; i++)
      fprintf(stderr, 
	      "Opt-Feature: %s\n", 
	      (osm_head->optional_features)[i]?(osm_head->optional_features)[i]:"[zero]");
    
    osmpbf__header_block__free_unpacked(osm_head, 0);
  } else 
    
    // parse OSMData
    if(!strcmp("OSMData", type)) {
      OSMPBF__PrimitiveBlock* pb = osmpbf__primitive_block__unpack(0, len, zd);
      if(pb) {
	fprintf(stderr, "primitive block \n\tn-primitive: %d\n", pb->n_primitivegroup);
	// Converting an integer to a lattitude or longitude uses the formula: OUT = IN * granularity / 10**9$
	if(pb->has_lat_offset && pb->has_lat_offset)
	  fprintf(stderr, "\toffset [%8lld, %8lld]\n", pb->lat_offset, pb->lon_offset);
	if(pb->has_granularity)
	  fprintf(stderr, "\tgranularity: %d\n", pb->granularity);
	// Granularity of dates, normally represented in units of milliseconds since the 1970 epoch.
	if(pb->has_date_granularity)
	  fprintf(stderr, "\tdate granularity: %d\n", pb->date_granularity);

	int i = 0;
	for(; i < pb->n_primitivegroup; i++) {
	  OSMPBF__PrimitiveGroup* pg = pb->primitivegroup[i];
	  fprintf(stderr, 
		  "Primitive Group %d:\n\tn-nodes: %d\n\tdense nodes: %s\n\tn-ways: %d\n\tn-relations: %d\n\tn-changesets: %d\n", 
		  i, pg->n_nodes, 
		  pg->dense?"yes":"no",
		  pg->n_ways, 
		  pg->n_relations, 
		  pg->n_changesets);
	  
	  if(pg->n_nodes > 0) {
	    assert(pg->nodes != 0);
	    for(int n = 0; n < pg->n_nodes; n++) {
	      OSMPBF__Node* node = pg->nodes[n];
	      // TODO get all amenity=bench nodes...
	      fprintf(stderr, "node %lld (keys: %d, vals: %d, [%8lld, %8lld])\n", 
		      node->id, node->n_keys, node->n_vals, node->lat, node->lon);
	      fprint_info(stderr, node->info);
	    }
	  }
	  if(pg->dense) {
	    OSMPBF__DenseNodes *dn = pg->dense;
	    fprintf(stderr, "Dense Nodes (id: %d, lat: %d, lon: %d, key: %d)\n", dn->n_id, dn->n_lat, dn->n_lon, dn->n_keys_vals);
	    for(int j=0; j < dn->n_lat && j < dn->n_lon; j++) {
	      //	      fprintf(stderr, "Dense Node %lld [%8lld, %8lld]\n", (dn->id)[j], dn->lat[j], dn->lon[j]);
	      }
	    
	  }
	  if(pg->n_relations > 0 && pg->relations ) {
	    int r;
	    for(r = 0; r < pg->n_relations; r++) {
	      OSMPBF__Relation* rel = pg->relations[r];
	      fprintf(stderr,"rel %lld (key: %d, val: %d, role: %d, mem: %d, types: %d)\n",
		      rel->id, rel->n_keys, rel->n_vals, rel->n_roles_sid, 
		      rel->n_memids, rel->n_types);
	      // print keys: 
	      /* 
		 for(int kv=0; kv < rel->n_keys && kv < rel->n_vals; kv++) {
		 char* key = stringtable_get_string(pb->stringtable, rel->keys[kv]);
		 char* val = stringtable_get_string(pb->stringtable, rel->vals[kv]);
		 fprintf(stderr, "\tkey/val: %s (%d) - %s (%d)\n", 
		 key?key:"", rel->keys[kv], val?val:"", rel->vals[kv]);
		 free(key);
		 free(val);
		 }
	      */
	      for(int mem = 0; mem < rel->n_memids; mem++){
		// TODO
	      }
	    }
	  }
	  if(pg->n_ways > 0 && pg->ways) {
	    int w;
	    for(w = 0; w < pg->n_ways; w++) {
	      OSMPBF__Way* way = pg->ways[w];
	      // way->info
	      fprintf(stderr, "way %8lld (keys %d, vals: %d, refs: %d)\n", 
		      way->id, way->n_keys, way->n_vals,  way->n_refs);
	      for(int k = 0; k < way->n_keys; k++) {
		if(k < way->n_vals) {
		  char* key = stringtable_get_string(pb->stringtable, way->keys[k]);
		  char* val = stringtable_get_string(pb->stringtable, way->vals[k]);
		  fprintf(stderr, "\tkey/val: %s (%d) - %s (%d)\n", 
			  key?key:"", way->keys[k], val?val:"", way->vals[k]);
		  free(key);
		  free(val);
		}
	      }
	       if(way->info)
		 fprint_info(stderr, way->info);
	    }
	  }
	  
	  
	}
	
	osmpbf__primitive_block__free_unpacked(pb, 0);
      }
    }
  
  if(must_free)
    free(zd);
  //	if(osm_blob->has_obsolete_bzip2_data){}
  
  // return something
}






/* 
 * see http://wiki.openstreetmap.org/wiki/PBF_Format
 * see OSM-binary for lib
 */
int main(int argc, char ** argv) {
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
      unpack_osmblob(bp->b, bp->bh->type);
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
}



