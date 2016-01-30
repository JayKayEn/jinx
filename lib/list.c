#include <lib.h>
#include <kmm.h>
#include <list.h>
#include <errno.h>

#define LISTTYPE 1111
#define LISTNODETYPE 1112

/* a lightweight canary to prevent use after free errors */
#define ASSERT_LIST(lst) assert(lst->datatype == LISTTYPE)
#define ASSERT_LISTNODE(node) assert(node->datatype == LISTNODETYPE)

struct listnode {
    int datatype;
    void* val;
    struct listnode* next;
};

struct list {
    int datatype;
    struct listnode* head;
    struct listnode* tail;
    unsigned int size;
};

/* Allocates and returns a list node object containing given element */
struct listnode* listnode_create(void* newval);

struct listnode*
listnode_create(void* newval) {
    struct listnode* newnode;

    newnode = (struct listnode*)kmalloc(sizeof(struct listnode));
    if (newnode == NULL)
        return NULL;
    newnode->datatype = LISTNODETYPE;
    newnode->val = newval;
    newnode->next = NULL;

    return newnode;
}

struct list*
list_create(void) {
    struct list* newlist;

    newlist = (struct list*)kmalloc(sizeof(struct list));
    if (newlist == NULL)
        return NULL;

    newlist->datatype = LISTTYPE;
    newlist->head = NULL;
    newlist->tail = NULL;
    newlist->size = 0;

    return newlist;
}

int
list_push_back(struct list* lst, void* newval) {
    assert(lst != NULL);
    ASSERT_LIST(lst);

    struct listnode* newnode = listnode_create(newval);
    if (newnode == NULL)
        return ENOMEM;
    ASSERT_LISTNODE(newnode);

    if (lst->size == 0)
        lst->head = newnode;
    else {
        ASSERT_LISTNODE(lst->tail);
        lst->tail->next = newnode;
    }
    lst->tail = newnode;

    ++lst->size;

    return 0;
}

void
list_pop_front(struct list* lst) {
    assert(lst != NULL);
    assert(lst->datatype == LISTTYPE);

    if (lst->size == 0)
        return;

    struct listnode* old_head = lst->head;
    ASSERT_LISTNODE(old_head);
    lst->head = lst->head->next;
    --lst->size;

    if (lst->size == 0)
        lst->tail = NULL;

    kfree(old_head);
}

void*
list_front(struct list* lst) {
    assert(lst != NULL);
    ASSERT_LIST(lst);

    if (lst->size == 0)
        return NULL;

    assert(lst->head != NULL);
    ASSERT_LISTNODE(lst->head);
    return lst->head->val;
}

void*
list_find(struct list* lst, void* query_val, int(*comparator)(void* left, void* right)) {
    assert(lst != NULL);
    ASSERT_LIST(lst);

    struct listnode* p = lst->head;
    while (p) {
        ASSERT_LISTNODE(p);
        if (comparator(p->val, query_val) == 0)
            return p->val;
        p = p->next;
    }
    return NULL;
}

void*
list_remove(struct list* lst, void* query_val, int(*comparator)(void* left, void* right)) {
    assert(lst != NULL);
    ASSERT_LIST(lst);

    void* res = NULL;
    struct listnode* p;
    struct listnode* q = NULL;
    for (p = lst->head; p != NULL; q = p, p = p->next) {
        ASSERT_LISTNODE(p);
        if (comparator(p->val, query_val) == 0) {
            if (q == NULL) {
                /* Removing from head */
                lst->head = p->next;
            } else {
                /* Removing after head. */
                q->next = p->next;
            }
            if (p == lst->tail)
                lst->tail = q;
            res = p->val;
            kfree(p);
            --lst->size;
            break;
        }
    }
    if (lst->size == 0)
        lst->tail = NULL;
    return res;
}

int
list_isempty(struct list* lst) {
    assert(lst != NULL);
    ASSERT_LIST(lst);

    return (lst->size == 0);
}

unsigned int
list_getsize(struct list* lst) {
    assert(lst != NULL);
    ASSERT_LIST(lst);

    return (lst->size);
}

void
list_destroy(struct list* lst) {
    if (lst != NULL) {
        ASSERT_LIST(lst);

        struct listnode* p = lst->head;
        struct listnode* q;
        /* frees every listnode object in the list */
        while (p != NULL) {
            ASSERT_LISTNODE(p);
            q = p->next;
            kfree(p);
            p = q;
        }
    }
    /* frees the list object itself */
    kfree(lst);
}

void
list_assertvalid(struct list* lst) {
    assert(lst != NULL);
    ASSERT_LIST(lst);
    /* Validate if the stated number of items in the list is correct. */
    unsigned int count = 0;
    struct listnode* p;
    struct listnode* prev = NULL;
    for (p = lst->head; p != NULL; p = p->next) {
        ASSERT_LISTNODE(p);
        ++count;
        prev = p;
    }
    /* Validate if the tail is reachable from the head. */
    if (count == 0)
        assert(lst->tail == NULL);
    else
        assert(prev == lst->tail);
    assert(count == lst->size);
}
