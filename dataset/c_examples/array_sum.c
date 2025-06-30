#include <stdio.h>

int main() {
    int arr[] = {1, 2, 3, 4, 5};
    int length = sizeof(arr) / sizeof(arr[0]);
    int sum = 0;
    for (int i = 0; i < length; i++) {
        sum += arr[i];
    }
    double avg = (double)sum / length;
    printf("Sum = %d, Average = %.2f\n", sum, avg);
    return 0;
}
