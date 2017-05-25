#include <assert.h>
#include <ctype.h> 
#include <errno.h> 
#include <math.h>
#include <stddef.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "patricia.h"

/*
 * prefix_tochar convert prefix information to bytes 
 */
u_char         *
prefix_tochar(prefix_t * prefix)
{
    if (prefix == NULL)
        return (NULL);

    return ((u_char *) & prefix->add.sin);
}

static inline int
comp_with_mask(void *addr, void *dest, u_int mask)
{

    if ( /* mask/8 == 0 || */ memcmp(addr, dest, mask / 8) == 0) {
        if (mask % 8 == 0)
            return 1;

        int             n = mask / 8;
        int             m = ((-1) << (8 - (mask % 8)));

        if ((((u_char *) addr)[n] & m) == (((u_char *) dest)[n] & m))
            return (1);
    }
    return (0);
}

/*
 * this allows imcomplete prefix 
 */
int
my_inet_pton(int af, const char *src, void *dst)
{
    if (af == AF_INET) {
        int             i,
                        c,
                        val;
	/* not thread safe */
        u_char          xp[4] = { 0, 0, 0, 0 };

        for (i = 0;; i++) {
            c = *src++;
            if (!isdigit(c))
                return (-1);
            val = 0;
            do {
                val = val * 10 + c - '0';
                if (val > 255)
                    return (0);
                c = *src++;
            } while (c && isdigit(c));
            xp[i] = val;
            if (c == '\0')
                break;
            if (c != '.')
                return (0);
            if (i >= 3)
                return (0);
        }
        memcpy(dst, xp, 4);
        return (1);
    } else if (af == AF_INET6) {
        return (inet_pton (af, src, dst));
    } else {
        errno = EAFNOSUPPORT;
        return -1;
    }
}

/*
 * convert prefix information to ascii string with length
 * thread safe and (almost) re-entrant implementation
 */
char           *
prefix_toa2x(prefix_t * prefix, char *buff, int with_len)
{
    if (prefix == NULL)
        return ("(Null)");
    assert(prefix->ref_count >= 0);
    if (buff == NULL) {

        struct buffer {
            char            buffs[16][48 + 5];
            u_int           i;
        }              *buffp;

	/* not thread safe */
        static struct buffer local_buff;
        buffp = &local_buff;

        if (buffp == NULL) {
            return (NULL);
        }

        buff = buffp->buffs[buffp->i++ % 16];
    }
    if (prefix->family == AF_INET) {
        u_char         *a;
        assert(prefix->bitlen <= 32);
        a = prefix_touchar(prefix);
        if (with_len) {
            sprintf(buff, "%d.%d.%d.%d/%d", a[0], a[1], a[2], a[3],
                    prefix->bitlen);
        } else {
            sprintf(buff, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
        }
        return (buff);
    } else if (prefix->family == AF_INET6) {
	char *r;
	r = (char *) inet_ntop (AF_INET6, &prefix->add.sin6, buff, 48 /* a guess value */ );
	if (r && with_len) {
	    assert (prefix->bitlen <= 128);
	    sprintf (buff + strlen (buff), "/%d", prefix->bitlen);
	}
	return (buff);
    } else
        return (NULL);
}

/*
 * prefix_toa2 convert prefix information to ascii string 
 */
char           *
prefix_toa2(prefix_t * prefix, char *buff)
{
    return (prefix_toa2x(prefix, buff, 0));
}

char           *
prefix_toa(prefix_t * prefix)
{
    return (prefix_toa2(prefix, (char *) NULL));
}

prefix_t       *
New_Prefix2(int family, void *dest, int bitlen,
            prefix_t * prefix)
{
    int             dynamic_allocated = 0;
    int             default_bitlen = 32;

    if (family == AF_INET) {
        if (prefix == NULL) {
            prefix = calloc(1, sizeof(prefix4_t));
            dynamic_allocated++;
        }
        memcpy(&prefix->add.sin, dest, 4);
    } else if (family == AF_INET6) {
        default_bitlen = 128;
	if (prefix == NULL) {
            prefix = calloc(1, sizeof (prefix6_t));
	    dynamic_allocated++;
	}
	memcpy (&prefix->add.sin6, dest, 16);
    } else {
        return (NULL);
    }

    prefix->bitlen = (bitlen >= 0) ? bitlen : default_bitlen;
    prefix->family = family;
    prefix->ref_count = 0;
    if (dynamic_allocated) {
        prefix->ref_count++;
    }
    return (prefix);
}

prefix_t       *
New_Prefix(int family, void *dest, int bitlen)
{
    return (New_Prefix2(family, dest, bitlen, NULL));
}

/*
 * ascii2prefix 
 */
prefix_t       *
ascii2prefix2(int family, char *string, prefix_t *prefix)
{
    u_long          bitlen,
                    maxbitlen = 0;
    char           *cp;
    struct in_addr  sin;
    struct in6_addr sin6;
    int             result;
    /*
     * not thread safe 
     */
    char save[MAXLINE];

    if (string == NULL)
    {
        return (NULL);
    }

    if (family == 0) {
       family = AF_INET;
       if (strchr (string, ':')) family = AF_INET6;
    }

    if (family == AF_INET) {
        maxbitlen = 32;
    }
    else if (family == AF_INET6) {
        maxbitlen = 128;
    }

    if ((cp = strchr(string, '/')) != NULL) {
        bitlen = atol(cp + 1);
        assert(cp - string < MAXLINE);
        memcpy(save, string, cp - string);
        save[cp - string] = '\0';
        string = save;
        if (bitlen > maxbitlen)
            bitlen = maxbitlen;
    } else {
        bitlen = maxbitlen;
    }

    if (family == AF_INET) {
        if ((result = my_inet_pton(AF_INET, string, &sin)) <= 0)
	{
            return (NULL);
	}
        return (New_Prefix2(AF_INET, &sin, bitlen, prefix));
    } else if (family == AF_INET6) {
        if ((result = inet_pton (AF_INET6, string, &sin6)) <= 0)
            return (NULL);
	return (New_Prefix2(AF_INET6, &sin6, bitlen, prefix));
    } else
        return (NULL);
}

prefix_t       *
ascii2prefix(int family, char *string) {
    return ascii2prefix2(family, string, NULL);
}

prefix_t       *
Ref_Prefix(prefix_t * prefix)
{
    if (prefix == NULL)
        return (NULL);
    if (prefix->ref_count == 0) {
        /*
         * make a copy in case of a static prefix 
         */
        return (New_Prefix2
                (prefix->family, &prefix->add, prefix->bitlen,
                 NULL));
    }
    prefix->ref_count++;
    return (prefix);
}

void
Deref_Prefix(prefix_t * prefix)
{
    if (prefix == NULL)
        return;
    /*
     * for secure programming, raise an assert. no static prefix can call
     * this 
     */
    assert(prefix->ref_count > 0);

    prefix->ref_count--;
    assert(prefix->ref_count >= 0);
    if (prefix->ref_count <= 0) {
        free(prefix);
        return;
    }
}


/* not thread safe, nor is it really used */
// static int      num_active_patricia = 0;

/*
 * these routines support continuous mask only 
 */

patricia_tree_t *
New_Patricia(int maxbits)
{
    patricia_tree_t *patricia = calloc(1, sizeof *patricia);
    patricia->maxbits = maxbits;
    patricia->head = NULL;
    patricia->num_active_node = 0;
    assert(maxbits <= PATRICIA_MAXBITS);        /* XXX */
    //num_active_patricia++;
    return (patricia);
}


/*
 * if func is supplied, it will be called as func(node->data)
 * before deleting the node
 */

void
Clear_Patricia(patricia_tree_t * patricia, void_fn_t func)
{
    assert(patricia);
    if (patricia->head) {

	/* not thread safe */
        patricia_node_t *Xstack[PATRICIA_MAXBITS + 1];
        patricia_node_t **Xsp = Xstack;
        patricia_node_t *Xrn = patricia->head;

        while (Xrn) {
            patricia_node_t *l = Xrn->l;
            patricia_node_t *r = Xrn->r;

            if (Xrn->prefix) {
                Deref_Prefix(Xrn->prefix);
                if (Xrn->data && func)
                    func(Xrn->data);
            } else {
                assert(Xrn->data == NULL);
            }
            free(Xrn);
            patricia->num_active_node--;

            if (l) {
                if (r) {
                    *Xsp++ = r;
                }
                Xrn = l;
            } else if (r) {
                Xrn = r;
            } else if (Xsp != Xstack) {
                Xrn = *(--Xsp);
            } else {
                Xrn = (patricia_node_t *) 0;
            }
        }
    }
    assert(patricia->num_active_node == 0);
}


void
Destroy_Patricia(patricia_tree_t * patricia, void_fn_t func)
{
    Clear_Patricia(patricia, func);
    free(patricia);
    //num_active_patricia--;
}


/*
 * if func is supplied, it will be called as func(node->prefix, node->data)
 */

void
patricia_process(patricia_tree_t * patricia, void_fn_t func)
{
    patricia_node_t *node;
    assert(func);

    PATRICIA_WALK(patricia->head, node) {
        func(node->prefix, node->data);
    } PATRICIA_WALK_END;
}

size_t
patricia_walk_inorder(patricia_node_t * node, void_fn_t func)
{
    size_t          n = 0;
    assert(func);

    if (node->l) {
        n += patricia_walk_inorder(node->l, func);
    }

    if (node->prefix) {
        func(node->prefix, node->data);
        n++;
    }

    if (node->r) {
        n += patricia_walk_inorder(node->r, func);
    }

    return n;
}


patricia_node_t *
patricia_search_exact(patricia_tree_t * patricia, prefix_t * prefix)
{
    patricia_node_t *node;
    u_char         *addr;
    u_int           bitlen;

    assert(patricia);
    assert(prefix);
    assert(prefix->bitlen <= patricia->maxbits);

    if (patricia->head == NULL)
        return (NULL);

    node = patricia->head;
    addr = prefix_touchar(prefix);
    bitlen = prefix->bitlen;

    while (node->bit < bitlen) {

        if (BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
            node = node->r;
        } else {
            node = node->l;
        }

        if (node == NULL)
            return (NULL);
    }

    if (node->bit > bitlen || node->prefix == NULL)
        return (NULL);
    assert(node->bit == bitlen);
    assert(node->bit == node->prefix->bitlen);
    if (comp_with_mask(prefix_tochar(node->prefix), prefix_tochar(prefix),
                       bitlen)) {
        return (node);
    }
    return (NULL);
}


/*
 * if inclusive != 0, "best" may be the given prefix itself 
 */
patricia_node_t *
patricia_search_best2(patricia_tree_t * patricia, prefix_t * prefix,
                      int inclusive)
{
    patricia_node_t *node;
    patricia_node_t *stack[PATRICIA_MAXBITS + 1];
    u_char         *addr;
    u_int           bitlen;
    int             cnt = 0;

    if (!patricia || !prefix)
	return NULL;

    assert(patricia);
    assert(prefix);
    assert(prefix->bitlen <= patricia->maxbits);

    if (patricia->head == NULL)
        return (NULL);

    node = patricia->head;
    addr = prefix_touchar(prefix);
    bitlen = prefix->bitlen;

    while (node->bit < bitlen) {

        if (node->prefix) {
            stack[cnt++] = node;
        }

        if (BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
            node = node->r;
        } else {
            node = node->l;
        }

        if (node == NULL)
            break;
    }

    if (inclusive && node && node->prefix)
        stack[cnt++] = node;

    if (cnt <= 0)
        return (NULL);

    while (--cnt >= 0) {
        node = stack[cnt];
        if (comp_with_mask(prefix_tochar(node->prefix),
                           prefix_tochar(prefix), node->prefix->bitlen)) {
            return (node);
        }
    }
    return (NULL);
}

/*
 * if inclusive != 0, "best" may be the given prefix itself 
 */
int
patricia_search_all(patricia_tree_t * patricia, prefix_t * prefix, int inclusive, patricia_node_t **results)
{
    patricia_node_t *node;
    u_char         *addr;
    u_int           bitlen;
    int             cnt = 0;

    node = patricia->head;
    addr = prefix_touchar(prefix);
    bitlen = prefix->bitlen;

    if (node == NULL)
        return 0;

    while (node->bit < bitlen) {

        if (node->prefix && node->data &&
            comp_with_mask(prefix_tochar(node->prefix),
                           prefix_tochar(prefix), node->prefix->bitlen)) {
            results[cnt++] = node;
        }

        if (BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
            node = node->r;
        } else {
            node = node->l;
        }

        if (!node)
            return cnt;
    }

    if (inclusive && node->prefix && node->data &&
        comp_with_mask(prefix_tochar(node->prefix),
                       prefix_tochar(prefix), node->prefix->bitlen)) {
        results[cnt++] = node;
    }

    return cnt;
}


patricia_node_t *
patricia_search_best(patricia_tree_t * patricia,
                     prefix_t * prefix)
{
    return (patricia_search_best2(patricia, prefix, 1));
}


patricia_node_t *
patricia_lookup(patricia_tree_t * patricia,
                prefix_t * prefix)
{
    patricia_node_t *node,
                   *new_node,
                   *parent,
                   *glue;
    u_char         *addr,
                   *test_addr;
    u_int           bitlen,
                    check_bit,
                    differ_bit;
    int             i,
                    j,
                    r;


    if (!patricia || ! prefix || prefix->bitlen > patricia->maxbits)
	return NULL;
#if 0
    if (!patricia || !prefix || 
	    prefix->bitlen >= patricia->maxbits)
	return NULL;
    printf("%p %p\n", patricia, prefix);

    assert(patricia);
    assert(prefix);
    assert(prefix->bitlen <= patricia->maxbits);
#endif

    if (patricia->head == NULL) {
        node = calloc(1, sizeof *node);
        node->bit = prefix->bitlen;
        node->prefix = Ref_Prefix(prefix);
        node->parent = NULL;
        node->l = node->r = NULL;
        node->data = NULL;
        patricia->head = node;
        patricia->num_active_node++;
        return (node);
    }

    addr = prefix_touchar(prefix);
    bitlen = prefix->bitlen;
    node = patricia->head;

    while (node->bit < bitlen || node->prefix == NULL) {

        if (node->bit < patricia->maxbits &&
            BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
            if (node->r == NULL)
                break;
            node = node->r;
        } else {
            if (node->l == NULL)
                break;
            node = node->l;
        }

        assert(node);
    }

    assert(node->prefix);
    test_addr = prefix_touchar(node->prefix);
    /*
     * find the first bit different 
     */
    check_bit = (node->bit < bitlen) ? node->bit : bitlen;
    differ_bit = 0;
    for (i = 0; i * 8 < check_bit; i++) {
        if ((r = (addr[i] ^ test_addr[i])) == 0) {
            differ_bit = (i + 1) * 8;
            continue;
        }
        /*
         * I know the better way, but for now 
         */
        for (j = 0; j < 8; j++) {
            if (BIT_TEST(r, (0x80 >> j)))
                break;
        }
        /*
         * must be found 
         */
        assert(j < 8);
        differ_bit = i * 8 + j;
        break;
    }
    if (differ_bit > check_bit)
        differ_bit = check_bit;

    parent = node->parent;
    while (parent && parent->bit >= differ_bit) {
        node = parent;
        parent = node->parent;
    }

    if (differ_bit == bitlen && node->bit == bitlen) {
        if (node->prefix) {
            return (node);
        }
        node->prefix = Ref_Prefix(prefix);
        assert(node->data == NULL);
        return (node);
    }

    new_node = calloc(1, sizeof *new_node);
    new_node->bit = prefix->bitlen;
    new_node->prefix = Ref_Prefix(prefix);
    new_node->parent = NULL;
    new_node->l = new_node->r = NULL;
    new_node->data = NULL;
    patricia->num_active_node++;

    if (node->bit == differ_bit) {
        new_node->parent = node;
        if (node->bit < patricia->maxbits &&
            BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
            assert(node->r == NULL);
            node->r = new_node;
        } else {
            assert(node->l == NULL);
            node->l = new_node;
        }
        return (new_node);
    }

    if (bitlen == differ_bit) {
        if (bitlen < patricia->maxbits &&
            BIT_TEST(test_addr[bitlen >> 3], 0x80 >> (bitlen & 0x07))) {
            new_node->r = node;
        } else {
            new_node->l = node;
        }
        new_node->parent = node->parent;
        if (node->parent == NULL) {
            assert(patricia->head == node);
            patricia->head = new_node;
        } else if (node->parent->r == node) {
            node->parent->r = new_node;
        } else {
            node->parent->l = new_node;
        }
        node->parent = new_node;
    } else {
        glue = calloc(1, sizeof *glue);
        glue->bit = differ_bit;
        glue->prefix = NULL;
        glue->parent = node->parent;
        glue->data = NULL;
        patricia->num_active_node++;
        if (differ_bit < patricia->maxbits &&
            BIT_TEST(addr[differ_bit >> 3], 0x80 >> (differ_bit & 0x07))) {
            glue->r = new_node;
            glue->l = node;
        } else {
            glue->r = node;
            glue->l = new_node;
        }
        new_node->parent = glue;

        if (node->parent == NULL) {
            assert(patricia->head == node);
            patricia->head = glue;
        } else if (node->parent->r == node) {
            node->parent->r = glue;
        } else {
            node->parent->l = glue;
        }
        node->parent = glue;
    }
    return (new_node);
}


void
patricia_remove(patricia_tree_t * patricia, patricia_node_t * node)
{
    patricia_node_t *parent,
                   *child;

    assert(patricia);
    assert(node);

    if (node->r && node->l) {

        /*
         * this might be a placeholder node -- have to check and make sure
         * there is a prefix aossciated with it ! 
         */
        if (node->prefix != NULL)
            Deref_Prefix(node->prefix);
        node->prefix = NULL;
        /*
         * Also I needed to clear data pointer -- masaki 
         */
        node->data = NULL;
        return;
    }

    if (node->r == NULL && node->l == NULL) {
        parent = node->parent;
        Deref_Prefix(node->prefix);
        patricia->num_active_node--;

        if (parent == NULL) {
            assert(patricia->head == node);
            patricia->head = NULL;
            free(node);
            return;
        }

        free(node);
        if (parent->r == node) {
            parent->r = NULL;
            child = parent->l;
        } else {
            assert(parent->l == node);
            parent->l = NULL;
            child = parent->r;
        }

        if (parent->prefix)
            return;

        /*
         * we need to remove parent too 
         */

        if (parent->parent == NULL) {
            assert(patricia->head == parent);
            patricia->head = child;
        } else if (parent->parent->r == parent) {
            parent->parent->r = child;
        } else {
            assert(parent->parent->l == parent);
            parent->parent->l = child;
        }
        child->parent = parent->parent;
        free(parent);
        patricia->num_active_node--;
        return;
    }
    if (node->r) {
        child = node->r;
    } else {
        assert(node->l);
        child = node->l;
    }
    parent = node->parent;
    child->parent = parent;

    Deref_Prefix(node->prefix);
    patricia->num_active_node--;

    if (parent == NULL) {
        assert(patricia->head == node);
        patricia->head = child;
        free(node);
        return;
    }

    if (parent->r == node) {
        parent->r = child;
    } else {
        assert(parent->l == node);
        parent->l = child;
    }
    free(node);
}

patricia_node_t *
make_and_lookup(patricia_tree_t * tree, char *string)
{
    prefix_t       *prefix;
    patricia_node_t *node;

    prefix = ascii2prefix(0, string);
    node = patricia_lookup(tree, prefix);
    Deref_Prefix(prefix);
    return (node);
}

#ifdef NOTUSED
patricia_node_t *
try_search_best(patricia_tree_t * tree, char *string)
{
    prefix_t       *prefix;
    patricia_node_t *node;

    prefix = ascii2prefix(0, string);
    if ((node = patricia_search_best(tree, prefix)) == NULL)
        Deref_Prefix(prefix);
    return (node);
}
#endif
