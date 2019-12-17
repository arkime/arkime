typedef enum
{
  TEST_ENUM_FOO,
  TEST_ENUM_BAR,
  TEST_ENUM_BAZ,
  TEST_ENUM_QUUX
} TestEnum;

typedef enum
{
  TEST_FLAGS_NONE     = 0,
  TEST_FLAGS_MOURNING = (1 << 0),
  TEST_FLAGS_LAUGHING = (1 << 1),
  TEST_FLAGS_TALKING  = (1 << 2),
  TEST_FLAGS_WALKING  = (1 << 3)
} TestFlags;
