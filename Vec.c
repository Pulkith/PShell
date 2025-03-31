/**
 * From Penn-Vec
 *
 */

#include "./Vec.h"
#include <stdlib.h>
#include "./panic.h"

Vec vec_new(size_t initial_capacity, ptr_dtor_fn ele_dtor_fn) {
  // Capacity check
  if (initial_capacity < 0 || initial_capacity > __UINT32_MAX__) {
    panic("Invalid capacity\n");
  }

  // Intialize Vector struct
  Vec vec;

  // Malloc space for capacity amount of data
  ptr_t* data = malloc(sizeof(void*) * initial_capacity);
  if (data == NULL) {
    panic("Malloc failed\n");
  }

  // Set the variables
  vec.data = data;
  vec.capacity = initial_capacity;
  vec.ele_dtor_fn = ele_dtor_fn;
  vec.length = 0;

  // Return v
  return vec;
}

ptr_t vec_get(Vec* self, size_t index) {
  // Index bounds check
  if (index < 0 || index >= self->length) {
    panic("Index out of bounds 1\n");
  }

  // Return element at index
  return (self->data[index]);
}

void vec_set(Vec* self, size_t index, ptr_t new_ele) {
  // Index bounds check
  if (index < 0 || index >= self->length) {
    panic("Index out of bounds 2\n");
  }

  if (self->ele_dtor_fn != NULL) {
    self->ele_dtor_fn(self->data[index]);
  }

  // Set element at index
  self->data[index] = new_ele;
}

void vec_push_back(Vec* self, ptr_t new_ele) {
  size_t len = self->length;
  size_t capacity = self->capacity;

  // Resize if you need to
  if (len == capacity) {
    size_t new_capacity;
    if (capacity == 0) {
      new_capacity = 1;
    } else {
      new_capacity = capacity * 2;
    }
    vec_resize(self, new_capacity);
  }

  self->data[len] = new_ele;
  self->length++;
}

bool vec_pop_back(Vec* self) {
  if (self->length == 0) {
    return false;
  }
  // Destroy last element
  if (self->ele_dtor_fn != NULL) {
    self->ele_dtor_fn(self->data[self->length - 1]);
  }
  self->data[self->length - 1] = NULL;
  self->length--;
  return true;
}

void vec_insert(Vec* self, size_t index, ptr_t new_ele) {
  if (index < 0 || index > self->length) {
    panic("Index out of bounds 3\n");
  } else if (index == self->length) {
    vec_push_back(self, new_ele);
    return;
  }

  size_t len = self->length;
  size_t capacity = self->capacity;

  // Resize if you need to
  if (len == capacity) {
    size_t new_capacity;
    if (capacity == 0) {
      new_capacity = 1;
    } else {
      new_capacity = capacity * 2;
    }
    vec_resize(self, new_capacity);
  }

  // Shift over everything
  for (size_t i = len; i > index; i--) {
    self->data[i] = self->data[i - 1];
  }

  // Insert element at index
  self->data[index] = new_ele;
  self->length++;
}

void vec_erase(Vec* self, size_t index) {
  // Index bounds check
  if (index < 0 || index >= self->length) {
    panic("Index out of bounds 4\n");
  }

  // Destruct at index
  if (self->ele_dtor_fn != NULL) {
    self->ele_dtor_fn(self->data[index]);
  }

  // Shift down
  for (size_t i = index; i < self->length - 1; i++) {
    self->data[i] = self->data[i + 1];
  }

  // Decrement length
  self->length--;
}

void vec_resize(Vec* self, size_t new_capacity) {
  if (new_capacity <= self->length) {
    return;
  }
  // Malloc space for new amount of data
  ptr_t* data = malloc(sizeof(void*) * new_capacity);
  if (data == NULL) {
    panic("Malloc failed\n");
  }

  // Copy data
  for (int i = 0; i < self->length; i++) {
    data[i] = self->data[i];
    if (self->ele_dtor_fn != NULL) {
      self->ele_dtor_fn(self->data[i]);
    }
  }

  // Free previous data array
  free(self->data);

  // Replace data with bigger malloc
  self->data = data;

  // Set new capacity
  self->capacity = new_capacity;
}

void vec_clear(Vec* self) {
  // Apply custom element destructor
  for (int i = 0; i < self->length; i++) {
    if (self->ele_dtor_fn != NULL) {
      self->ele_dtor_fn(self->data[i]);
    }
  }

  // Set length to 0
  self->length = 0;

  // Free previous data array
  free(self->data);

  // Malloc new data array
  ptr_t* data = malloc(sizeof(void*) * self->capacity);
  if (data == NULL) {
    panic("Malloc failed\n");
  }

  // Reset data
  self->data = data;
}

void vec_destroy(Vec* self) {
  // Clear elements
  vec_clear(self);

  // Free allocated memory
  free(self->data);

  // Set data = null && capacity = 0
  self->data = NULL;
  self->capacity = 0;
}
