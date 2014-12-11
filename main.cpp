/* C++ proxy to get C++ rtl loaded; 'true' main is in vision.c */
extern "C" { int vision_main(int argc, char *argv[]); };
int main(int argc, char *argv[])
{
    return vision_main(argc, argv);
}

