# CSCI596-LZW-parallelization

## What's the algorithm
Data compression is a fundamental technique in system design and communication, aiming to reduce the size of data while retaining most of the original information. This process facilitates efficient storage and transmission of data, with the ability to retrieve the original data through decompression. Since the inception of modern computer systems, various algorithms have been developed for this purpose, and one notable algorithm is the Lempel–Ziv–Welch (LZW) algorithm, which is mainly used for GIF compression.

In this project, we will explore two different approaches to parallelizing the LZW algorithm. Our objective is to compare their performance against the serial baseline, analyze their efficiency concerning their design, and identify potential areas for improvement. This investigation will contribute insights into the parallelization of the LZW algorithm, shedding light on its complexities and offering avenues for enhancing its performance.

## How does the algorithm work?
The algorithm has two parts: Encode and decode.
### Encode
![lzw-encode](https://github.com/ManuShi98/CSCI596-LZW-parallelization/blob/master/Picture/lzw-encode.png?raw=true)

### Decode
![lzw-decode](https://github.com/ManuShi98/CSCI596-LZW-parallelization/blob/master/Picture/lzw-decode.jpg?raw=true)

## How will we parallelize it?
A naive idea based on [Parallel Lempel-Ziv-Welch (PLZW) Technique for
Data Compression](https://www.ijcsit.com/docs/Volume%203/vol3Issue3/ijcsit2012030340.pdf) is that we make the following changes to the algorithm.

For the encoding part, we seperate the string into several blocks according to the number of processors. Then run LZW on each processor. Each processor will have a offset so the index will begin with offset.

![parallel-encode](https://github.com/ManuShi98/CSCI596-LZW-parallelization/blob/master/Picture/encode-naive-parallel.png?raw=true)

For the decoding part, we assign the sequence to different processor based on the block size. 
![parallel-decode](https://github.com/ManuShi98/CSCI596-LZW-parallelization/blob/master/Picture/decode-naive-parallel.jpg?raw=true)

The naive idea is very fast. However, it's possible that the compression rate is bad because duplicated word is added to the dictionary. An alternative is proposed by (Parallel Lempel Ziv coding)[https://u.cs.biu.ac.il/~wisemay/dam2005.pdf]. In this paper authors mentioned that we can use a tree structure to run the LZW algorithm. Starting from the root node, we wait the parent node to finish it's encoding, then share the dictionary with its sons. This can help save the efficiency of compression.

![lzw-log](https://github.com/ManuShi98/CSCI596-LZW-parallelization/blob/master/Picture/lzw-log.jpg?raw=true)

## What's our expectation on the result?
There is latency, overhead in the system, we hope the final result will be like this.

![effiency](https://github.com/ManuShi98/CSCI596-LZW-parallelization/blob/master/Picture/efficiency.jpg?raw=true)