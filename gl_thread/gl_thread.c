#include "gl_thread.h"
#include <stdio.h>

void init_glthread(glthread_t *glthread)
{
    glthread->left = NULL;
    glthread->right = NULL;
}

void
glthread_add_next(glthread_t *base_gl, glthread_t *new_gl)
{
    if(!base_gl->right) {
        base_gl->right = new_gl;
        new_gl->left = base_gl;
        return;
    }
    
    glthread_t *temp = base_gl->right;
    base_gl->right = new_gl;
    new_gl->left = base_gl;
    new_gl->right = temp;
    temp->left = new_gl;
}
void
glthread_add_before(glthread_t *base_gl, glthread_t *new_gl)
{
    if(!base_gl->left) {
        new_gl->left = NULL;
        base_gl->left = new_gl;
        new_gl->right = base_gl;
        return;
    }
    
    glthread_t *temp = base_gl->left;
    base_gl->left = new_gl;
    new_gl->right = base_gl;
    new_gl->left = temp;
    temp->right = new_gl;
}
void
glthread_add_last(glthread_t *base_gl, glthread_t *new_gl)
{
    glthread_t *glthread_ptr = NULL;
    glthread_t *prev_glthread_ptr = NULL;
    
    ITERATE_GLTHREAD_BEGIN(base_gl, glthread_ptr) {
        prev_glthread_ptr = glthread_ptr;
    }ITERATE_GLTHREAD_END(base_gl, glthread_ptr);
    
    if(prev_glthread_ptr) {
        glthread_add_next(prev_glthread_ptr, new_gl);
    }else {
        glthread_add_next(base_gl, new_gl);
    }
}

void remove_glthread(glthread_t *glthread)
{
    if(!glthread->left) {
        if(glthread->right) {
            glthread->right->left = NULL;
            glthread->right = NULL;
            return;
        }
        return;
    }
    if(!glthread->right)
    {
        glthread->left->right = NULL;
        glthread->left = NULL;
        return;
    }
    
    
    glthread->left->right = glthread->right;
    glthread->right->left = glthread->left;
    glthread->left = NULL;
    glthread->right = NULL;
}

void delete_glthread_list(glthread_t *base_glthread)
{
    glthread_t *glthreadptr = NULL;
    ITERATE_GLTHREAD_BEGIN(base_glthread, glthreadptr)
    {
        remove_glthread(glthreadptr);
    }ITERATE_GLTHREAD_END(base_glthread, glthreadptr);
}
unsigned int
get_glthread_list_count(glthread_t *base_glthread)
{
    unsigned int count = 0;
    glthread_t *glthreadptr = NULL;
    ITERATE_GLTHREAD_BEGIN(base_glthread, glthreadptr) {
        ++count;
    }ITERATE_GLTHREAD_END(base_glthread, glthreadptr);
    
    return count;
}
void
glthread_priority_insert(glthread_t *glthread_head,
                         glthread_t *glthread,
                         int (*comp_fn)(void *, void *),
                         int offset)
{
    glthread_t *curr = NULL;
    glthread_t *prev = NULL;

    init_glthread(glthread);

    if(IS_GLTHREAD_LIST_EMPTY(glthread_head)) {
        glthread_add_next(glthread_head, glthread);
        return;
    }

    if(glthread_head->right && !glthread_head->right->right) {
        if(comp_fn(GLTHREAD_GET_USER_DATA_FROM_OFFSET(glthread, offset), 
            GLTHREAD_GET_USER_DATA_FROM_OFFSET(glthread_head->right, offset)) == -1)
        {
            glthread_add_next(glthread_head, glthread);
        }
        else {
            glthread_add_next(glthread_head->right, glthread);
        }
        return;
    }

    ITERATE_GLTHREAD_BEGIN(glthread_head, curr) {
         if(comp_fn(GLTHREAD_GET_USER_DATA_FROM_OFFSET(glthread, offset), 
            GLTHREAD_GET_USER_DATA_FROM_OFFSET(curr, offset)) != -1)
        {
            prev = curr;
            continue;
        }

        if(!prev) {
            glthread_add_next(glthread_head, glthread);
        } else {
            glthread_add_next(prev, glthread);
        }

        return;
    } ITERATE_GLTHREAD_END(glthread_head, curr) 

    glthread_add_next(prev, glthread);
}

glthread_t *
dequeue_glthread_first(glthread_t *base_glthread)
{
    glthread_t *temp = NULL;
    if(!base_glthread->right) return NULL;
    temp = base_glthread->right;
    remove_glthread(temp);
    return temp;
}

