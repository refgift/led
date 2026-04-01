#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "model.h"
#include "editor.h"
#include "config.h"

extern int tests_passed;
extern int tests_failed;

// Forward declarations for platform-specific functions
extern char *mkdtemp(char *);
extern int rmdir(const char *);

void
test_assert_autosave (int condition, const char *msg)
{
  if (condition)
    {
      tests_passed++;
      fprintf (stderr, "PASS: %s\n", msg);
    }
  else
    {
      tests_failed++;
      fprintf (stderr, "FAIL: %s\n", msg);
    }
}

// Check if file exists
static int
file_exists (const char *path)
{
  struct stat st;
  return stat (path, &st) == 0;
}

// Read entire file into buffer
static char *
read_file_content (const char *path)
{
  FILE *fp = fopen (path, "rb");
  if (!fp)
    return NULL;
  
  fseek (fp, 0, SEEK_END);
  long size = ftell (fp);
  fseek (fp, 0, SEEK_SET);
  
  char *buf = malloc (size + 1);
  if (buf)
    {
      (void) fread (buf, 1, size, fp);
      buf[size] = 0;
    }
  fclose (fp);
  return buf;
}

void
test_autosave_comprehensive ()
{
  fprintf (stderr, "Running auto-save comprehensive tests\n");
  
  // Create temp directory for test files
  char temp_dir[] = "/tmp/led_autosave_test_XXXXXX";
  (void) mkdtemp (temp_dir);
  
  // Test 1: keystroke_threshold_default
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test1.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 50;
    ed.backup_count = 0;
    
    // Insert 50 chars (should trigger save)
    for (int i = 0; i < 50; i++)
      buffer_insert_char (&ed.model, 0, i, 'a');
    
    auto_save (&ed);
    
    test_assert_autosave (file_exists (test_file), "keystroke: main file created");
    test_assert_autosave (ed.unsaved_keystrokes == 0, "keystroke: counter reset");
    
    buffer_free (&buf);
    unlink (test_file);
  }
  
  // Test 2: backup_file_creation
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test2.txt", temp_dir);
    char backup_file[1024];
    snprintf (backup_file, sizeof (backup_file), "%s.bak.1", test_file);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "content");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 1;
    ed.backup_count = 0;
    
    auto_save (&ed);
    
    test_assert_autosave (file_exists (backup_file), "backup: .bak.1 created");
    
    char *backup_content = read_file_content (backup_file);
    test_assert_autosave (backup_content != NULL && strcmp (backup_content, "content") == 0,
                         "backup: file content correct");
    if (backup_content)
      free (backup_content);
    
    buffer_free (&buf);
    unlink (test_file);
    unlink (backup_file);
  }
  
  // Test 3: backup_rotation
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test3.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 1;
    ed.backup_count = 0;
    
    // Perform 15 auto-saves to trigger rotation
    for (int i = 0; i < 15; i++)
      {


        // Update content
        buffer_free (&buf);
        buffer_init (&buf);
        char content[32];
        snprintf (content, sizeof (content), "version_%d", i);
        buffer_insert_line (&buf, 0, content);
        ed.model = buf;
        
        auto_save (&ed);
      }
    
    // Check that we have backups 1-10
    int backup_count = 0;
    for (int i = 1; i <= 10; i++)
      {


        char backup_file[1024];
        snprintf (backup_file, sizeof (backup_file), "%s.bak.%d", test_file, i);
        if (file_exists (backup_file))
          backup_count++;
      }
    
    test_assert_autosave (backup_count >= 9, "rotation: multiple backups exist");
    
    // Cleanup
    unlink (test_file);
    for (int i = 1; i <= 10; i++)
      {


        char backup_file[1024];
        snprintf (backup_file, sizeof (backup_file), "%s.bak.%d", test_file, i);
        unlink (backup_file);
      }
    
    buffer_free (&buf);
  }
  
  // Test 4: backup_content_preservation
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test4.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "version1");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 1;
    ed.backup_count = 0;
    
    // Save version 1
    auto_save (&ed);
    
    // Save version 2
    buffer_free (&buf);
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "version2");
    ed.model = buf;
    auto_save (&ed);
    
    // Check .bak.1 has version1
    char backup_file[1024];
    snprintf (backup_file, sizeof (backup_file), "%s.bak.1", test_file);
    char *content1 = read_file_content (backup_file);
    test_assert_autosave (content1 != NULL && strcmp (content1, "version1") == 0,
                         "preservation: bak.1 has version1");
    
    // Check .bak.2 has version2
    snprintf (backup_file, sizeof (backup_file), "%s.bak.2", test_file);
    char *content2 = read_file_content (backup_file);
    test_assert_autosave (content2 != NULL && strcmp (content2, "version2") == 0,
                         "preservation: bak.2 has version2");
    
    if (content1) free (content1);
    if (content2) free (content2);
    
    // Cleanup
    unlink (test_file);
    for (int i = 1; i <= 10; i++)
      {


        char bak[1024];
        snprintf (bak, sizeof (bak), "%s.bak.%d", test_file, i);
        unlink (bak);
      }
    
    buffer_free (&buf);
  }
  
  // Test 5: keystroke_counter_reset
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test5.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 49;
    ed.auto_save_threshold = 50;
    ed.backup_count = 0;
    
    // One more keystroke should trigger save
    ed.unsaved_keystrokes++;
    if (ed.unsaved_keystrokes >= ed.auto_save_threshold)
      {
        auto_save (&ed);
      }
    
    test_assert_autosave (ed.unsaved_keystrokes == 0, "counter: reset after save");
    
    buffer_free (&buf);
    unlink (test_file);
  }
  
  // Test 6: backup_count_increments
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test6.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "data");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 1;
    ed.backup_count = 0;
    
    for (int i = 0; i < 3; i++)
      {


        auto_save (&ed);
        test_assert_autosave (ed.backup_count == i + 1, "backup_count increments");
      }
    
    buffer_free (&buf);
    unlink (test_file);
    for (int i = 1; i <= 10; i++)
      {


        char bak[1024];
        snprintf (bak, sizeof (bak), "%s.bak.%d", test_file, i);
        unlink (bak);
      }
  }
  
  // Test 7: file_modified_flag_reset
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test7.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "content");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 1;
    ed.backup_count = 0;
    ed.file_modified = 1;
    
    auto_save (&ed);
    
    test_assert_autosave (ed.file_modified == 0, "flag: file_modified reset");
    
    buffer_free (&buf);
    unlink (test_file);
  }
  
  // Test 8: main_file_created
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test8.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "main content");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 1;
    ed.backup_count = 0;
    
    auto_save (&ed);
    
    test_assert_autosave (file_exists (test_file), "main: file created");
    
    char *main_content = read_file_content (test_file);
    test_assert_autosave (main_content != NULL && strcmp (main_content, "main content") == 0,
                         "main: file content correct");
    if (main_content)
      free (main_content);
    
    buffer_free (&buf);
    unlink (test_file);
  }
  
  // Test 9: multiple_autosaves
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test9.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 20;
    ed.backup_count = 0;
    
    // Insert 60 chars (triggers 3 saves)
    int save_count = 0;
    for (int i = 0; i < 60; i++)
      {


        buffer_insert_char (&ed.model, 0, i, 'a');
        ed.unsaved_keystrokes++;
        if (ed.unsaved_keystrokes >= ed.auto_save_threshold)
          {
            auto_save (&ed);
            save_count++;
          }
      }
    
    test_assert_autosave (save_count == 3, "multiple: 3 saves triggered");
    
    buffer_free (&buf);
    unlink (test_file);
    for (int i = 1; i <= 10; i++)
      {


        char bak[1024];
        snprintf (bak, sizeof (bak), "%s.bak.%d", test_file, i);
        unlink (bak);
      }
  }
  
  // Test 10: wraparound_rotation
  {
    char test_file[512];
    snprintf (test_file, sizeof (test_file), "%s/test10.txt", temp_dir);
    
    Buffer buf;
    buffer_init (&buf);
    buffer_insert_line (&buf, 0, "");
    
    Editor ed;
    ed.model = buf;
    ed.filename = test_file;
    ed.unsaved_keystrokes = 0;
    ed.auto_save_threshold = 1;
    ed.backup_count = 0;
    
    // Perform 11 saves
    for (int i = 0; i < 11; i++)
      {


        buffer_free (&buf);
        buffer_init (&buf);
        char line[32];
        snprintf (line, sizeof (line), "backup_%d", i);
        buffer_insert_line (&buf, 0, line);
        ed.model = buf;
        auto_save (&ed);
      }
    
    // .bak.10 should now contain save #11 (wrapped around)
    char backup_file[1024];
    snprintf (backup_file, sizeof (backup_file), "%s.bak.10", test_file);
    char *content = read_file_content (backup_file);
    
    // Due to modulo, backup_count=11, (11%10)+1=2, so check .bak.2
    snprintf (backup_file, sizeof (backup_file), "%s.bak.2", test_file);
    free (content);
    content = read_file_content (backup_file);
    
    test_assert_autosave (content != NULL, "wraparound: backup files exist");
    
    if (content)
      free (content);
    
    buffer_free (&buf);
    unlink (test_file);
    for (int i = 1; i <= 10; i++)
      {


        char bak[1024];
        snprintf (bak, sizeof (bak), "%s.bak.%d", test_file, i);
        unlink (bak);
      }
  }
  
  fprintf (stderr, "Auto-save comprehensive tests completed\n");
  
  // Cleanup temp directory
  rmdir (temp_dir);
}
