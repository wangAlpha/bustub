## PROJECT #1 - BUFFER POOL



 Do not post your project on a public Github repository.

# OVERVIEW

During the semester, you will be building a new disk-oriented storage manager for the [BusTub](https://github.com/cmu-db/bustub) DBMS. Such a storage manager assumes that the primary storage location of the database is on disk.

The first programming project is to implement a **buffer pool** in your storage manager. The buffer pool is responsible for moving physical pages back and forth from main memory to disk. It allows a DBMS to support databases that are larger than the amount of memory that is available to the system. The buffer pool's operations are transparent to other parts in the system. For example, the system asks the buffer pool for a page using its unique identifier (`page_id_t`) and it does not know whether that page is already in memory or whether the system has to go retrieve it from disk.

**Your implementation will need to be thread-safe**. Multiple threads will be accessing the internal data structures at the same and thus you need to make sure that their critical sections are protected with [latches](https://stackoverflow.com/a/42464336) (these are called "locks" in operating systems).

You will need to implement the following two components in your storage manager:

- [**LRU Replacement Policy**](https://15445.courses.cs.cmu.edu/fall2020/project1/#replacer)
- [**Buffer Pool Manager**](https://15445.courses.cs.cmu.edu/fall2020/project1/#buffer-pool)

This is a single-person project that will be completed individually (i.e. no groups).

- **Release Date:** Sep 14, 2020
- **Due Date:** Sep 27, 2020 @ 11:59pm

# PROJECT SPECIFICATION

For each of the following components, we are providing you with stub classes that contain the API that you need to implement. You **should not** modify the signatures for the pre-defined functions in these classes. If you modify the signatures, the test code that we use for grading will break and you will get no credit for the project. You also **should not** add additional classes in the source code for these components. These components should be entirely self-contained.

If a class already contains data members, you should **not** remove them. For example, the `BufferPoolManager` contains `DiskManager` and `Replacer` objects. These are required to implement the functionality that is needed by the rest of the system. On the other hand, you may need to add data members to these classes in order to correctly implement the required functionality. You can also add additional helper functions to these classes. The choice is yours.

You are allowed to use any built-in [C++17 containers](http://en.cppreference.com/w/cpp/container) in your project unless specified otherwise. It is up to you to decide which ones you want to use. Note that these containers are not thread-safe and that you will need to include latches in your implementation to protect them. You may not bring in additional third-party dependencies (e.g. boost).

## TASK #1 - LRU REPLACEMENT POLICY
该组件负责跟踪缓冲池中的页面使用情况。

This component is responsible for tracking page usage in the buffer pool. You will implement a new sub-class called `LRUReplacer` in `src/include/buffer/lru_replacer.h` and its corresponding implementation file in `src/buffer/lru_replacer.cpp`. `LRUReplacer` extends the abstract `Replacer` class (`src/include/buffer/replacer.h`), which contains the function specifications.

The size of the `LRUReplacer` is the same as buffer pool since it contains placeholders for all of the frames in the `BufferPoolManager`. However, not all the frames are considered as in the `LRUReplacer`. The `LRUReplacer` is initialized to have no frame in it. Then, only the newly unpinned ones will be considered in the `LRUReplacer`.

You will need to implement the *LRU* policy discussed in the class. You will need to implement the following methods:

- `Victim(T*)` : 移除最久访问对象，并传参回去，返回`True`。如果集合为空，则返回 `False`。
- `Pin(T)` : 将 `frame`与`BufferPoolManager`进行交换。移除`LRUReplacer`中`frame`。
- `Unpin(T)` : 向集合加`frame`。
- `Size()` : This method returns the number of frames that are currently in the `LRUReplacer`.

The implementation details are up to you. You are allowed to use built-in STL containers. You can assume that you will not run out of memory, but you must make sure that the operations are thread-safe.

## TASK #2 - BUFFER POOL MANAGER

`BufferPoolManager`主要是从`DiskManager`取得数据库页，并将其存储在内存中。`BufferPoolManager`也能在有明确的指令或者需要换出空间时。

Next, you need to implement the buffer pool manager in your system (`BufferPoolManager`). The `BufferPoolManager` is responsible for fetching database pages from the `DiskManager` and storing them in memory. The `BufferPoolManager` can also write dirty pages out to disk when it is either explicitly instructed to do so or when it needs to evict a page to make space for a new page.

为了确保您的实现与系统的其余部分一起正常工作，我们将为您提供一些已经填充的功能。您也不需要实现实际将数据读取和写入磁盘的代码（这称为我们实现中的`DiskManager`）。我们将为您提供该功能。

To make sure that your implementation works correctly with the rest of the system, we will provide you with some of the functions already filled in. You will also not need to implement the code that actually reads and writes data to disk (this is called the `DiskManager` in our implementation). We will provide that functionality for you.

系统中的所有内存页面都由`Page`对象表示。 `BufferPoolManager` 不需要理解这些页面的内容。但是对于系统开发人员来说，了解`Page`对象只是缓冲池中内存的容器，因此并不特定于唯一页面，这一点很重要。也就是说，每个 `Page` 对象都包含一块内存，`DiskManager` 将使用该内存块作为一个位置来复制它从磁盘读取的 **物理页面** 的内容。 `BufferPoolManager` 将重用相同的 `Page` 对象来存储数据，因为它来回移动到磁盘。这意味着在系统的整个生命周期中，同一个`Page`对象可能包含不同的物理页面。 `Page` 对象的标识符 (`page_id`) 跟踪它包含的物理页面；如果“Page”对象不包含物理页面，则其`page_id`必须设置为`INVALID_PAGE_ID`。

All in-memory pages in the system are represented by `Page` objects. The `BufferPoolManager` does not need to understand the contents of these pages. But it is important for you as the system developer to understand that `Page` objects are just containers for memory in the buffer pool and thus are not specific to a unique page. That is, each `Page` object contains a block of memory that the `DiskManager` will use as a location to copy the contents of a **physical page** that it reads from disk. The `BufferPoolManager` will reuse the same `Page` object to store data as it moves back and forth to disk. This means that the same `Page` object may contain a different physical page throughout the life of the system. The `Page` object's identifer (`page_id`) keeps track of what physical page it contains; if a `Page` object does not contain a physical page, then its `page_id` must be set to `INVALID_PAGE_ID`.

每个`Page`页维持一个计数器计数具有"pinned"页的线程数。`BufferPoolManager`不允许释放 pinned  的`Page`。每个`Page`object也要进行追踪。你的工作是记录页unpinned前是否被修改。 你的`BufferPoolManager`必须把脏页写回硬盘，在对象被重用前。

Each `Page` object also maintains a counter for the number of threads that have "pinned" that page. Your `BufferPoolManager` is not allowed to free a `Page` that is pinned. Each `Page` object also keeps track of whether it is dirty or not. It is your job to record whether a page was modified before it is unpinned. Your `BufferPoolManager` must write the contents of a dirty `Page` back to disk before that object can be reused.

`BufferPoolManager`使用 `LRUReplacer`实现。它将使用`LRUReplacer`来跟踪“Page”对象何时被访问，以便它可以决定在必须释放帧以腾出空间从磁盘复制新物理页面时驱逐哪个对象。

Your `BufferPoolManager` implementation will use the `LRUReplacer` class that you created in the previous steps of this assignment. It will use the `LRUReplacer` to keep track of when `Page` objects are accessed so that it can decide which one to evict when it must free a frame to make room for copying a new physical page from disk.

You will need to implement the following functions defined in the header file (`src/include/buffer/buffer_pool_manager.h`) in the source file (`src/buffer/buffer_pool_manager.cpp`):

- `FetchPageImpl(page_id)`
- `NewPageImpl(page_id)`
- `UnpinPageImpl(page_id, is_dirty)`
- `FlushPageImpl(page_id)`
- `DeletePageImpl(page_id)`
- `FlushAllPagesImpl()`

For FetchPageImpl,you should return NULL if no page is available in the free list and all other pages are currently pinned. FlushPageImpl should flush a page regardless of its pin status.

Refer to the function documentation for details on how to implement these functions. Don't touch the non-impl versions, we need those to grade your code.

# INSTRUCTIONS

See the [Project #0 instructions](https://15445.courses.cs.cmu.edu/fall2020/project0/#instructions) on how to create your private repository and setup your development environment.

## TESTING

You can test the individual components of this assigment using our testing framework. We use [GTest](https://github.com/google/googletest) for unit test cases. There are two separate files that contain tests for each component:

- `LRUReplacer`: `test/buffer/lru_replacer_test.cpp`
- `BufferPoolManager`: `test/buffer/buffer_pool_manager_test.cpp`

You can compile and run each test individually from the command-line:

```bash
$ mkdir build
$ cd build
$ make lru_replacer_test
$ ./test/lru_replacer_test
```

You can also run `make check-tests` to run ALL of the test cases. Note that some tests are disabled as you have not implemented future projects. You can disable tests in GTest by adding a `DISABLED_` prefix to the test name.

**Important:** These tests are only a subset of the all the tests that we will use to evaluate and grade your project. You should write additional test cases on your own to check the complete functionality of your implementation.

## FORMATTING

Your code must follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). We use [Clang](https://clang.llvm.org/) to automatically check the quality of your source code. Your project grade will be **zero** if your submission fails any of these checks.

Execute the following commands to check your syntax. The `format` target will automatically correct your code. The `check-lint` and `check-clang-tidy` targets will print errors and instruct you how to fix it to conform to our style guide.

```bash
$ make format
$ make check-lint
$ make check-clang-tidy
```

## DEVELOPMENT HINTS

Instead of using `printf` statements for debugging, use the `LOG_*` macros for logging information like this:

```bash
LOG_INFO("# Pages: %d", num_pages);
LOG_DEBUG("Fetching page %d", page_id);
```



To enable logging in your project, you will need to reconfigure it like this:

```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=DEBUG ..
$ make
```



The different logging levels are defined in `src/include/common/logger.h`. After enabling logging, the logging level defaults to `LOG_LEVEL_INFO`. Any logging method with a level that is equal to or higher than `LOG_LEVEL_INFO` (e.g., `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`) will emit logging information. Note that you will need to add `#include "common/logger.h"` to any file that you want to use the logging infrastructure.

We encourage you to use `gdb` to debug your project if you are having problems.

 Post all of your questions about this project on Piazza. Do **not** email the TAs directly with questions.

# GRADING RUBRIC

Each project submission will be graded based on the following criteria:

1. Does the submission successfully execute all of the test cases and produce the correct answer?
2. Does the submission execute without any memory leaks?
3. Does the submission follow the code formatting and style policies?

Note that we will use additional test cases to grade your submission that are more complex than the sample test cases that we provide you.

# LATE POLICY

See the [late policy](https://15445.courses.cs.cmu.edu/fall2020/syllabus.html#late-policy) in the syllabus.

# SUBMISSION

After completing the assignment, you can submit your implementation to Gradescope:

- **https://www.gradescope.com/courses/163907/**

You only need to include the following files:

- `src/include/buffer/lru_replacer.h`
- `src/buffer/lru_replacer.cpp`
- `src/include/buffer/buffer_pool_manager.h`
- `src/buffer/buffer_pool_manager.cpp`

You can submit your answers as many times as you like and get immediate feedback.

 CMU students should use the Gradescope course code announced on Piazza.

# COLLABORATION POLICY

- Every student has to work individually on this assignment.
- Students are allowed to discuss high-level details about the project with others.
- Students are **not** allowed to copy the contents of a white-board after a group meeting with other students.
- Students are **not** allowed to copy the solutions from another colleague.