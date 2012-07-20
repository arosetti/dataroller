#include "../bitio.h"

int main(int argc, char **argv)
{
   bitio *b = NULL;
   uint64_t data = 0x0123456789ABCDEF; //UINT64_MAX;
   uint64_t data2 = 0x0;

   //data = 0; //0x0123456789ABCDEF; /* UINT64_MAX; */

   printf("uint64_t data: %016llX -> %" PRIu64 "\n",
         (long long unsigned int)data,
         data);

   printf("uint64_t data2: %016llX -> %" PRIu64 "\n",
         (long long unsigned int)data2,
         data2 );

   if (!(b = bitio_open("test2", O_WRONLY)))
   {
       perror("bitio_open");
       exit(-1);
   }

   for(int i = 0; i<1000; i++ )
   {
       bitio_write(b, data, 12);
   }

   bitio_close(b);
   return 0;
}
