#include "../bitio.h"

int main(int argc, char **argv)
{
   bitio *b = NULL;
   uint64_t data = UINT64_MAX;
   int ret;
   
   if (!(b = bitio_open("test2", O_RDONLY)))
   {
       perror("bitio_open");
       exit(-1);
   }

   for(int i = 0; i<1; i++ )
   {
       ret = bitio_read(b, &data, 32);
       ret = bitio_read(b, &data, 64);
       if (ret)
       {
           printf("ret %d\n", ret);
           break;
       }
       fprintf(stderr,"%016llX -> %" PRIu64 "\n",
              (long long unsigned int)data,
              data);
        ret = bitio_read(b, &data, 32);

       /*ret = bitio_read(b, &data, 64);
       if (ret)
       {
           printf("ret %d\n", ret);
           break;
       }
       fprintf(stderr,"%016llX -> %" PRIu64 "\n",
              (long long unsigned int)data,
              data);*/
   }
   bitio_close(b);

   return 0;
}
