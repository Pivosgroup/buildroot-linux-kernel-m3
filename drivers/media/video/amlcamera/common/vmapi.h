#ifndef VM_API_INCLUDE_
#define VM_API_INCLUDE_

typedef struct vm_output_para{
	int width;
	int height;
	int bytesperline;
	int v4l2_format;
	int index;
	int v4l2_memory;
	int zoom;     // set -1 as invalid
	int mirror;   // set -1 as invalid
	int angle;
	unsigned vaddr;
}vm_output_para_t;

extern int vm_fill_buffer(struct videobuf_buffer* vb, vm_output_para_t* para);
#endif /* VM_API_INCLUDE_ */
