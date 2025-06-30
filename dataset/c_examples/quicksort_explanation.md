1: Include the standard input-output header file
3: Define a helper function to swap two integers by pointer
4: Store first value in a temporary variable
5: Copy second value into first location
6: Copy temporary value into second location
8: Define partition function to place pivot in correct position
9: Select last element as pivot
10: Initialize index of smaller element
11: Iterate from low to high - 1
12: If current element is less than pivot
13: Increment index and swap current element with element at index
16: After loop, swap element at index+1 with pivot
17: Return the partition index
19: Define quicksort function using recursion
20: If the low index is less than high
21: Partition the array and get pivot position
22: Recursively sort elements before pivot
23: Recursively sort elements after pivot
26: Begin the main function
27: Declare and initialize an integer array
28: Determine the number of elements in the array
29: Call quicksort on the entire array
30: Loop through the sorted array
31: Print each element followed by a space
33: Print a newline at the end
34: Return 0 to indicate success
35: End of main function
