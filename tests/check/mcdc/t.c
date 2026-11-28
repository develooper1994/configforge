/* The fixture of check.mcdc: one decision of two conditions, called once
   with both true. Two of its four condition outcomes are covered: 50.0%. */
int f(int a, int b) {
    return a && b;
}

int main(void) {
    return f(1, 1) ? 0 : 1;
}
