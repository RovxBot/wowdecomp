/* Native config for the bundled Expat 1.95.5, separate from winconfig.h.
 * Current native targets are little endian. No external entity resolver or DTD
 * functionality is installed; the XML adapter rejects DTD declarations. */
#define BYTEORDER 1234
#define HAVE_MEMMOVE 1
#define XML_NS 1
#define XML_CONTEXT_BYTES 1024
