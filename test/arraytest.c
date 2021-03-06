#include <lib.h>
#include <array.h>
#include <test.h>

#define TESTSIZE 73

static
void
testa(struct array* a) {
    int testarray[TESTSIZE];
    int i, j, n, r, *p;

    for (i = 0; i < TESTSIZE; i++)
        testarray[i] = i;

    n = array_num(a);
    assert(n == 0);

    for (i = 0; i < TESTSIZE; i++) {
        r = array_add(a, &testarray[i], NULL);
        assert(r == 0);
        n = array_num(a);
        assert(n == i + 1);
    }
    n = array_num(a);
    assert(n == TESTSIZE);

    for (i = 0; i < TESTSIZE; i++) {
        p = array_get(a, i);
        assert(*p == i);
    }
    n = array_num(a);
    assert(n == TESTSIZE);

    for (j = 0; j < TESTSIZE * 4; j++) {
        i = random() % TESTSIZE;
        p = array_get(a, i);
        assert(*p == i);
    }
    n = array_num(a);
    assert(n == TESTSIZE);

    for (i = 0; i < TESTSIZE; i++)
        array_set(a, i, &testarray[TESTSIZE - i - 1]);

    for (i = 0; i < TESTSIZE; i++) {
        p = array_get(a, i);
        assert(*p == TESTSIZE - i - 1);
    }

    r = array_setsize(a, TESTSIZE / 2);
    assert(r == 0);

    for (i = 0; i < TESTSIZE / 2; i++) {
        p = array_get(a, i);
        assert(*p == TESTSIZE - i - 1);
    }

    array_remove(a, 1);

    for (i = 1; i < TESTSIZE / 2 - 1; i++) {
        p = array_get(a, i);
        assert(*p == TESTSIZE - i - 2);
    }
    p = array_get(a, 0);
    assert(*p == TESTSIZE - 1);

    array_setsize(a, 2);
    p = array_get(a, 0);
    assert(*p == TESTSIZE - 1);
    p = array_get(a, 1);
    assert(*p == TESTSIZE - 3);

    array_set(a, 1, NULL);
    array_setsize(a, 2);
    p = array_get(a, 0);
    assert(*p == TESTSIZE - 1);
    p = array_get(a, 1);
    assert(p == NULL);

    array_setsize(a, TESTSIZE * 10);
    p = array_get(a, 0);
    assert(*p == TESTSIZE - 1);
    p = array_get(a, 1);
    assert(p == NULL);
}

int
arraytest(int nargs, char** args) {
    struct array* a;

    (void)nargs;
    (void)args;

    print("Beginning array test...\n");
    a = array_create();
    assert(a != NULL);

    testa(a);

    array_setsize(a, 0);

    testa(a);

    array_setsize(a, 0);
    array_destroy(a);

    print("Array test complete\n");
    return 0;
}
