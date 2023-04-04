#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

#define MAX_LENGTH 500
#define OF_CHECK 9223372036854775808

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

struct bn {
    int cur_size;
    int max_size;
    unsigned long long *digits;
};

static struct bn *bn_init(int k)
{
    if (k <= 0) {
        return NULL;
    }

    struct bn *new = kmalloc(sizeof(struct bn), GFP_KERNEL);
    if (!new) {
        return NULL;
    }

    new->digits = kmalloc(sizeof(long long) * k, GFP_KERNEL);
    if (!new->digits) {
        kfree(new);
        return NULL;
    }

    memset(new->digits, 0ULL, sizeof(long long) * k);
    new->max_size = k;
    new->cur_size = 1;
    return new;
}

static void bn_release(struct bn *a)
{
    kfree(a->digits);
    kfree(a);
}

static void bn_swap(struct bn *a, struct bn *b)
{
    if (!a || !b) {
        return;
    }

    struct bn tmp = *a;
    *a = *b;
    *b = tmp;
}

static void bn_add(struct bn *dst, struct bn *a, struct bn *b)
{
    if (!dst || !a || !b) {
        return;
    }
    /*b->cur_size must bigger than a->cur_size*/
    if (a->cur_size > b->cur_size) {
        bn_swap(a, b);
    }
    int carry = 0;

    for (int i = 0; i < b->cur_size; i++) {
        if (i >= a->cur_size) {
            dst->digits[i] = b->digits[i] + carry;
            if (b->digits[i] + carry == 0ULL) {
                carry = 1;
            } else {
                carry = 0;
            }
        } else {
            int last_carry = carry;
            if (a->digits[i] + carry > ~b->digits[i]) {
                carry = 1;
            } else {
                carry = 0;
            }
            dst->digits[i] = a->digits[i] + b->digits[i] + last_carry;
        }
    }
    dst->cur_size = b->cur_size;
    if (carry) {
        dst->digits[b->cur_size] = 1ULL;
        dst->cur_size++;
    }
}

static void bn_shift(struct bn *dst, int k, struct bn *a)
{
    for (int i = 0; i < k; i++) {
        int carry = 0;
        for (int j = 0; j < a->cur_size; j++) {
            dst->digits[j] = a->digits[j] << 1;
            if (carry) {
                dst->digits[j]++;
            }
            if (a->digits[j] & OF_CHECK) {
                carry = 1;
            } else {
                carry = 0;
            }
        }

        dst->cur_size = a->cur_size;
        if (carry) {
            dst->digits[a->cur_size] = 1ULL;
            dst->cur_size++;
        }
    }
}

static void bn_sub(struct bn *dst, struct bn *a, struct bn *b)
{
    if (a->cur_size < b->cur_size) {
        bn_swap(a, b);
    }

    int borrow = 0;
    unsigned long long mll = -1;
    for (int i = 0; i < a->cur_size; i++) {
        if (i >= b->cur_size) {
            dst->digits[i] = a->digits[i];
            if (borrow) {
                if (a->digits[i] == 0) {
                    borrow = 1;
                } else {
                    borrow = 0;
                }
                dst->digits[i]--;
            }
        } else {
            if (a->digits[i] - borrow < b->digits[i] ||
                (borrow == 1 && a->digits[i] == 0)) {
                dst->digits[i] =
                    (mll - b->digits[i] - borrow) + a->digits[i] + 1;
                borrow = 1;
            } else {
                dst->digits[i] = a->digits[i] - b->digits[i] - borrow;
                borrow = 0;
            }
        }
    }

    for (int i = dst->max_size - 1; i >= 0; i--) {
        if (dst->digits[i]) {
            dst->cur_size = i + 1;
            break;
        }
    }
}

static void bn_mul(struct bn *dst, struct bn *a, struct bn *b)
{
    /*a or b cannot equal to dst*/
    for (int i = 0; i < b->cur_size; i++) {
        unsigned __int128 res = 0;
        unsigned long long carry = 0ULL;
        for (int j = 0; j < a->cur_size; j++) {
            res = (unsigned __int128) a->digits[j] *
                      (unsigned __int128) b->digits[i] +
                  carry;
            carry = res >> 64;
            if (dst->digits[j + i] > (long long) ~res) {
                carry++;
            }
            dst->digits[j + i] += (long long) res;
        }
        if (carry) {
            dst->digits[a->cur_size + i] = carry;
        }
    }

    for (int i = dst->max_size - 1; i >= 0; i--) {
        if (dst->digits[i]) {
            dst->cur_size = i + 1;
            break;
        }
    }
}

static struct bn *fib_sequence(long long k)
{
    if (k < 0) {
        return NULL;
    }
    int ll_size = (-181 + k * 109) / 1000 + 1;
    struct bn *a = bn_init(ll_size);
    struct bn *b = bn_init(ll_size);
    b->digits[0] = 1ULL;

    for (int i = 2; i <= k; i++) {
        bn_add(a, a, b);
        bn_swap(a, b);
    }

    if (k == 0) {
        bn_swap(a, b);
    }

    bn_release(a);
    return b;
}

static struct bn *fib_fast_doubling(long long k)
{
    unsigned int h = 0;
    for (long long i = k; i; h++, i >>= 1)
        ;

    int ll_size = (-181 + k * 109) / 1000 + 1;
    struct bn *a = bn_init(ll_size);
    struct bn *b = bn_init(ll_size);
    struct bn *c = bn_init(ll_size);
    struct bn *tmp = bn_init(ll_size);
    struct bn *d = bn_init(ll_size);
    b->digits[0] = 1ULL;

    for (long long mask = 1 << (h - 1); mask; mask >>= 1) {
        // c = a * (2 * b - a);188
        // d = a * a + b * b189
        memset(c->digits, 0ULL, sizeof(long long) * ll_size);
        bn_shift(tmp, 1, b);
        bn_sub(tmp, tmp, a);
        bn_mul(c, a, tmp);

        memset(d->digits, 0ULL, sizeof(long long) * ll_size);
        memset(tmp->digits, 0ULL, sizeof(long long) * ll_size);
        bn_mul(d, a, a);
        bn_mul(tmp, b, b);
        bn_add(d, d, tmp);

        if (mask & k) {
            bn_add(b, c, d);
            bn_swap(a, d);
        } else {
            bn_swap(a, c);
            bn_swap(b, d);
        }
    }

    bn_release(b);
    bn_release(d);
    bn_release(tmp);
    bn_release(c);
    return a;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    struct bn *a;
    switch (size) {
    case 0:
        a = fib_sequence(*offset);
        break;
    case 1:
        a = fib_fast_doubling(*offset);
        break;
    }
    size_t sz = a->cur_size * sizeof(unsigned long long);
    copy_to_user(buf, a->digits, sz);
    bn_release(a);
    return sz;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    ktime_t kt;
    struct bn *a;
    switch (size) {
    case 0:
        kt = ktime_get();
        a = fib_sequence(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 1:
        kt = ktime_get();
        a = fib_fast_doubling(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    }
    bn_release(a);
    return (ssize_t) ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);