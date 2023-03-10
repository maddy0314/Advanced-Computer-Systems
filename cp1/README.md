# Advanced Computer Systems Class Project 1
## Compression Using Multithreading
### Overview
This project uses libraries pthread and ZSTD to compress a text file using multithreading. The file is processed threadwise in 16kb blocks. The number of threads and level of compression are user-configurable on the command line.
### Compiling and Running
You will need to download from this repository:
```
main.c
common.h
```
You will also need an input file located in the same folder as the above. You must install ZSTD on your machine before compiling this program - ZSTD's public repository is located at https://github.com/facebook/zstd.

This project can be compiled using the following line:
```
gcc -g main.c -lzstd -I/usr/include/zstd -L/usr/lib -pthread -o main.out
```
After compiling, the project can be executed as follows:
```
./main.out <input_file> <compression_level> <num_threads>
```
The arguments are, in order: the name of the input file as it appears in your directory, your desired ZSTD compression level (1-20, where 20 is the most compressed), and the number of worker threads you would like to initialize.

The output file generated by the program will be in the format ```.txt.zst```. You can uncompress this file using WinRAR or other extraction programs, or from the command line using ZSTD itself by entering ```unzstd filename.txt.zst```.

## Design
This project uses the streaming compression functionality of ZSTD. The following is a broad overview of its workings. ```main.c``` is highly commented such that it should be easy to follow along with this framework when reading the code.

Main Function Operations:
1) Open input and output files, and initialize an array of threads and an array of thread wrapper structs (see below)
2) Read the input file to a buffer 16kB at a time
3) Initialize a thread for each 16kB chunk up to the thread number configured by the user
4) Join all active threads and write their results to the output file
5) Repeat steps 2 through 4 until the entire input file has been read
6) Cleanup and free memory

Thread Compression Function Operations:
1) Initialize compression context for this thread
2) Set up ZSTD input and output buffers for a single chunk
3) Read 16kB from the input buffer, compress it, and write it to the output buffer

The thread wrapper struct serves to pass relevant buffering information to the thread, since threads can only be passed one argument during their creation.

### Results and Analysis
The following graph was generated using an input .txt file of 25MB. Execution was timed using the time command when running the project in Ubuntu on WSL. Data points were taken at 1-10, 15, 20, 25, 50, 75, and 100 threads.
![Real Time (s) vs  Threads](https://user-images.githubusercontent.com/98151091/215096855-5f79ca90-77e0-4f20-a6ad-408d8426ebbd.png)

Extreme gains in time efficiency are observed from 1 to 5 threads - it takes 0.508 seconds to run the program with just one thread, but only 0.016 seconds to run it with five. Gains are more minimal from there - it takes 0.014 seconds to run with 20 threads, and the lowest I could get it was to around 0.010 seconds at 50 threads. It is likely that time stabilizes at about 5 threads because creating and managing more threads is costly - the computational load of making more threads is probably balancing out any increases in efficiency they might have conferred.

### Shortcomings and Improvements
+ At present, the .txt.zst output files produced by this program are corrupted. It is imperative to understand why this is happening and modify the program to produce decompressable files.
+ Using DrMemory on this code displays a few minor memory leaks. Finding and patching these would increase efficiency.
+ It is possible using ZSTD to share the compression context between threads such that it only needs to be initialized once. This would make the program more time- and memory-efficient. I attempted to implement this, but ran into fatal memory errors that I did not have time to resolve properly.
