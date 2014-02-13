#ifndef AUTOSTR_H
#define AUTOSTR_H

/**
 * Initial size to allocate for each instance's char pointer, and size by which
 * to increase or decrease the allocated space when more or less is needed.
 */
#define AUTOSTR_CHUNK 256

/**
 * Structure that represents the autostr. All fields are private; use
 * the autostr_len and autostr_value functions instead.
 */
typedef struct {
    size_t size;
    size_t len;
    char *ptr;
} autostr;

/**
 * Allocate and return a new autostr instance.
 */
autostr *autostr_new();

/**
 * Deallocate the instance and set the pointer to NULL.
 */
void autostr_free(autostr **instance);

/**
 * Get the instance's string.
 */
const char *autostr_value(autostr *instance);

/**
 * Get the length of the string. This operation runs in O(1) time.
 */
int autostr_len(autostr *instance);

/**
 * If instance points to a NULL pointer, assign it to a newly allocated
 * autostr. Otherwise, reset the existing instance to a blank string.
 */
autostr *autostr_recycle(autostr **instance);

/**
 * Append a string to the instance.
 */
autostr *autostr_append(autostr *instance, const char *append);

/**
 * Append a single char to the instance.
 */
autostr *autostr_push(autostr *instance, char push);

/**
 * Trim whitespace from the start of the string.
 */
autostr *autostr_ltrim(autostr *instance);

/**
 * Trim whitespace from the end of the string.
 */
autostr *autostr_rtrim(autostr *instance);

/**
 * Trim whitespace from both sides of the string.
 */
autostr *autostr_trim(autostr *instance);

/**
 * Compare the instance's value to the other value.
 */
int autostr_cmp(autostr *instance, const char *other);

/**
 * Replace each character `c` in the string with the return value of func(c).
 * Providing tolower or toupper as the function yields the expected result.
 */
autostr *autostr_apply(autostr *instance, int func(int));

#endif