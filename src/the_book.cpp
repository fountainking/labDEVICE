#include "the_book.h"
#include "ui.h"
#include "settings.h"
#include "ascii_art.h"
#include <algorithm>

// Global state
BookState bookState = BOOK_SEARCH;
String searchInput = "";
std::vector<SearchResult> searchResults;
int selectedResultIndex = 0;
int articleScrollOffset = 0;
String currentArticleTitle = "";
std::vector<String> articleLines;
int currentCategoryIndex = 0;

// Search debouncing
unsigned long lastSearchInputTime = 0;
const unsigned long SEARCH_DEBOUNCE_MS = 100;  // Wait 100ms after typing stops (faster!)
bool searchPending = false;

// Categories for The Book (all 15 categories)
BookCategory categories[] = {
  {"Wikipedia", "/the_book/wikipedia", TFT_BLUE},
  {"Science", "/the_book/science", TFT_CYAN},
  {"History", "/the_book/history", TFT_ORANGE},
  {"Literature", "/the_book/literature", TFT_MAGENTA},
  {"Religious Texts", "/the_book/religious", TFT_PURPLE},
  {"Medical", "/the_book/medical", TFT_RED},
  {"Survival", "/the_book/survival", TFT_YELLOW},
  {"Programming", "/the_book/programming", TFT_GREEN},
  {"Prog Languages", "/the_book/programming_languages", TFT_DARKGREEN},
  {"Edible Plants", "/the_book/edible_plants", TFT_GREENYELLOW},
  {"Poisonous Plants", "/the_book/poisonous_plants", TFT_MAROON},
  {"Agriculture", "/the_book/agriculture", TFT_OLIVE},
  {"DIY/Technical", "/the_book/diy_technical", TFT_NAVY},
  {"Reference", "/the_book/reference", TFT_DARKGREY},
  {"Tech", "/the_book/tech", TFT_LIGHTGREY}
};
const int totalCategories = 15;

// Display constants
const int RESULTS_PER_PAGE = 8;
const int LINES_PER_PAGE = 7;  // Lines of text that fit on screen (size 2 font)

void enterTheBook() {
  bookState = BOOK_SEARCH;
  searchInput = "";
  searchResults.clear();
  selectedResultIndex = 0;
  articleScrollOffset = 0;
  currentCategoryIndex = 0;

  // SD card is already initialized in setup() - just check if it's mounted
  if (SD.cardType() == CARD_NONE) {
    Serial.println(F("SD card not mounted for The Book"));
  }

  // Check if data folder exists
  if (!SD.exists("/the_book")) {
    Serial.println(F("WARNING: /the_book folder not found on SD card!"));
  }

  drawBookSearch();
}

void exitTheBook() {
  // CRITICAL FIX: Force deallocation of vectors (can be 10-15KB!)
  std::vector<SearchResult>().swap(searchResults);  // Force deallocation
  std::vector<String>().swap(articleLines);          // Force deallocation

  Serial.printf("Book cleanup: freed search results and article lines\n");
}

void drawBookSearch() {
  canvas.fillScreen(TFT_WHITE);

  // "Offline Knowledge Base" title (black, moved down)
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("Offline Knowledge Base", 50, 15);

  // Large rounded search bar
  canvas.drawRoundRect(15, 30, 210, 30, 15, TFT_BLACK);
  canvas.drawRoundRect(16, 31, 208, 28, 14, TFT_BLACK);  // Double outline

  // "Search:" label - black text
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("Search:", 25, 40);

  // Show search input with blinking cursor - black text
  String displayInput = searchInput;
  if (millis() % 1000 < 500) {
    displayInput += "_";
  }
  canvas.drawString(displayInput, 75, 40);

  // Show live search results preview under search bar
  if (searchResults.size() > 0 && searchInput.length() >= 1) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLACK);
    String preview = String(searchResults.size()) + " results - UP/DOWN to select";
    canvas.drawString(preview, 20, 70);

    // Calculate scroll window to keep selected result visible
    const int maxVisible = 5;  // Max results visible at once
    int startIdx = selectedResultIndex - maxVisible / 2;
    startIdx = max(0, min(startIdx, (int)searchResults.size() - maxVisible));
    startIdx = max(0, startIdx);  // Ensure non-negative
    int endIdx = min((int)searchResults.size(), startIdx + maxVisible);

    // Show scrolling results window
    int yPos = 85;
    for (int i = startIdx; i < endIdx; i++) {
      // Yellow highlight for selected result
      if (i == selectedResultIndex) {
        canvas.fillRect(18, yPos - 2, 204, 11, TFT_YELLOW);
      }

      canvas.setTextColor(TFT_BLACK);
      String title = "> " + searchResults[i].title;
      if (title.length() > 35) {
        title = title.substring(0, 32) + "...";
      }
      canvas.drawString(title, 20, yPos);
      yPos += 11;
    }
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawTableOfContents() {
  // Draw semi-transparent overlay (simulate by redrawing search screen first)
  drawBookSearch();

  // Draw popup window (taller to fit all categories)
  int popupX = 30;
  int popupY = 20;
  int popupW = 180;
  int popupH = 100;

  // Popup background (white with black border)
  canvas.fillRoundRect(popupX, popupY, popupW, popupH, 8, TFT_WHITE);
  canvas.drawRoundRect(popupX, popupY, popupW, popupH, 8, TFT_BLACK);
  canvas.drawRoundRect(popupX + 1, popupY + 1, popupW - 2, popupH - 2, 7, TFT_BLACK);

  // Title
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_RED);
  canvas.drawString("Table of Contents", popupX + 30, popupY + 8);

  // Draw categories list
  canvas.setTextSize(1);
  int yPos = popupY + 22;
  for (int i = 0; i < totalCategories; i++) {
    canvas.setTextColor(categories[i].color);
    String categoryText = "> " + categories[i].name;
    canvas.drawString(categoryText, popupX + 10, yPos);
    yPos += 11;
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawBookResults() {
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK);
  canvas.setTextSize(1);

  // Title
  canvas.setTextColor(TFT_RED);
  canvas.drawString("Search Results", 10, 5);

  // Results count
  canvas.setTextColor(TFT_BLACK);
  String countStr = "Found: " + String(searchResults.size()) + " results";
  canvas.drawString(countStr, 10, 15);

  if (searchResults.size() == 0) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("No results found", 10, 50);
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString("Try a different search term", 10, 65);
    canvas.drawString("` Back", 5, 120);
    return;
  }

  // Calculate scroll window
  int startIdx = max(0, selectedResultIndex - RESULTS_PER_PAGE / 2);
  int endIdx = min((int)searchResults.size(), startIdx + RESULTS_PER_PAGE);

  // Draw results
  int y = 30;
  for (int i = startIdx; i < endIdx; i++) {
    if (i == selectedResultIndex) {
      canvas.fillRect(5, y - 2, 230, 13, TFT_YELLOW);
    }

    // Truncate title if too long
    String title = searchResults[i].title;
    if (title.length() > 35) {
      title = title.substring(0, 32) + "...";
    }

    canvas.setTextColor(TFT_RED);
    canvas.drawString("> " + title, 8, y);

    // Category tag
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString("[" + searchResults[i].category + "]", 200, y);

    y += 13;
  }

  // Instructions
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("UP/DOWN: Navigate | ENTER: Open", 5, 120);
  canvas.drawString("` Back", 5, 130);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawBookArticle() {
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK);
  canvas.setTextSize(1);

  // Title bar
  canvas.fillRect(0, 0, 240, 18, TFT_YELLOW);
  canvas.setTextColor(TFT_BLACK);
  String truncTitle = currentArticleTitle;
  if (truncTitle.length() > 35) {
    truncTitle = truncTitle.substring(0, 32) + "...";
  }
  canvas.drawString(truncTitle, 5, 5);

  // Article content - SIZE 2 for readability!
  int y = 22;
  int startLine = articleScrollOffset;
  int endLine = min((int)articleLines.size(), startLine + LINES_PER_PAGE);

  canvas.setTextSize(2);  // BIGGER FONT
  for (int i = startLine; i < endLine; i++) {
    String line = articleLines[i];

    // Highlight markdown links/bold in red
    if (line.indexOf("**") >= 0 || line.indexOf("http") >= 0) {
      canvas.setTextColor(TFT_RED);
    } else {
      canvas.setTextColor(TFT_BLACK);
    }

    // Word wrap for long lines (size 2 = ~19 chars fit)
    if (line.length() > 19) {
      line = line.substring(0, 19);
    }

    canvas.drawString(line, 5, y);
    y += 14;  // Better spacing for size 2
  }

  // Scroll indicator (size 1 for compactness)
  canvas.setTextSize(1);
  if (articleLines.size() > LINES_PER_PAGE) {
    canvas.setTextColor(TFT_BLACK);
    String scrollInfo = String(startLine + 1) + "-" + String(endLine) + "/" + String(articleLines.size());
    canvas.drawString(scrollInfo, 190, 5);
  }

  // Instructions
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("UP/DOWN: Scroll | ` Back", 5, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void handleBookNavigation(char key) {
  if (bookState == BOOK_TOC) {
    // Any key closes the table of contents popup
    bookState = BOOK_SEARCH;
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
    drawBookSearch();
  } else if (bookState == BOOK_RESULTS) {
    if (key == ';' || key == ',') {
      // Navigate results up
      if (selectedResultIndex > 0) {
        selectedResultIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
        drawBookResults();
      }
    } else if (key == '.' || key == '/') {
      // Navigate results down
      if (selectedResultIndex < (int)searchResults.size() - 1) {
        selectedResultIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
        drawBookResults();
      }
    }
  } else if (bookState == BOOK_ARTICLE) {
    if (key == ';' || key == ',') {
      scrollArticleUp();
    } else if (key == '.' || key == '/') {
      scrollArticleDown();
    }
  }
}

std::vector<SearchResult> searchIndex(String indexPath, String query) {
  std::vector<SearchResult> results;

  // Check if index file exists
  if (!SD.exists(indexPath)) {
    Serial.println("Index file not found: " + indexPath);
    return results;
  }

  File indexFile = SD.open(indexPath, FILE_READ);
  if (!indexFile) {
    Serial.println("Failed to open index: " + indexPath);
    return results;
  }

  query.toLowerCase();

  // Three-tier priority ranking (Google-style)
  std::vector<SearchResult> startsWithMatches;  // Title STARTS with query (highest)
  std::vector<SearchResult> titleContainsMatches;  // Title CONTAINS query
  std::vector<SearchResult> contentMatches;  // Content contains query (lowest)

  int linesRead = 0;
  const int MAX_LINES_TO_READ = 150;  // Increased from 100 for better results
  const int MAX_RESULTS_PER_CATEGORY = 5;  // Limit to 5 per category (15 total max across 3 tiers)

  // Read index line by line (limited for responsiveness)
  while (indexFile.available() && linesRead < MAX_LINES_TO_READ &&
         (startsWithMatches.size() + titleContainsMatches.size() + contentMatches.size()) < MAX_RESULTS_PER_CATEGORY) {

    linesRead++;
    String line = indexFile.readStringUntil('\n');
    line.trim();

    // Skip empty lines
    if (line.length() == 0) continue;

    // Parse: KEYWORD|FILENAME|LINE|PREVIEW
    int pipe1 = line.indexOf('|');
    int pipe2 = line.indexOf('|', pipe1 + 1);
    int pipe3 = line.indexOf('|', pipe2 + 1);

    if (pipe1 > 0 && pipe2 > pipe1 && pipe3 > pipe2) {
      SearchResult result;
      result.title = line.substring(0, pipe1);
      result.filename = line.substring(pipe1 + 1, pipe2);
      result.lineNumber = line.substring(pipe2 + 1, pipe3).toInt();
      result.preview = line.substring(pipe3 + 1);

      // Extract category from path
      if (indexPath.indexOf("wikipedia") >= 0) {
        result.category = "Wiki";
      } else if (indexPath.indexOf("programming") >= 0) {
        result.category = "Code";
      } else if (indexPath.indexOf("survival") >= 0) {
        result.category = "Survival";
      } else if (indexPath.indexOf("religious") >= 0) {
        result.category = "Religious";
      } else if (indexPath.indexOf("edible") >= 0) {
        result.category = "Plants";
      } else {
        result.category = "Other";
      }

      // Smart ranking like Google
      String lowerTitle = result.title;
      lowerTitle.toLowerCase();
      int titleMatchPos = lowerTitle.indexOf(query);

      if (titleMatchPos == 0) {
        // HIGHEST PRIORITY: Title STARTS with query ("fi" -> "Fire", not "Goldfish")
        startsWithMatches.push_back(result);
      } else if (titleMatchPos > 0) {
        // MEDIUM PRIORITY: Title CONTAINS query
        titleContainsMatches.push_back(result);
      } else {
        // LOWEST PRIORITY: Only in content
        String lowerPreview = result.preview;
        lowerPreview.toLowerCase();
        if (lowerPreview.indexOf(query) >= 0) {
          contentMatches.push_back(result);
        }
      }
    }
  }

  // Sort each tier by title length (shorter = more relevant)
  std::sort(startsWithMatches.begin(), startsWithMatches.end(),
    [](const SearchResult& a, const SearchResult& b) {
      return a.title.length() < b.title.length();
    });
  std::sort(titleContainsMatches.begin(), titleContainsMatches.end(),
    [](const SearchResult& a, const SearchResult& b) {
      return a.title.length() < b.title.length();
    });

  // Combine results: starts-with first, then contains, then content (max 5 total to conserve memory)
  for (size_t i = 0; i < startsWithMatches.size() && results.size() < MAX_RESULTS_PER_CATEGORY; i++) {
    results.push_back(startsWithMatches[i]);
  }
  for (size_t i = 0; i < titleContainsMatches.size() && results.size() < MAX_RESULTS_PER_CATEGORY; i++) {
    results.push_back(titleContainsMatches[i]);
  }
  for (size_t i = 0; i < contentMatches.size() && results.size() < MAX_RESULTS_PER_CATEGORY; i++) {
    results.push_back(contentMatches[i]);
  }

  indexFile.close();
  return results;
}

void searchTheBook(String query) {
  if (query.length() < 2) {
    searchResults.clear();
    return;
  }

  searchResults.clear();
  selectedResultIndex = 0;

  const int MAX_TOTAL_RESULTS = 15;  // Hard limit to prevent memory bloat (was unlimited!)

  // SCRIPTURE VERSE DETECTION: Check if query looks like Bible verse (e.g., "John 3:16", "Gen 1:1")
  bool isVerseQuery = (query.indexOf(':') > 0 && query.indexOf(' ') > 0);

  if (isVerseQuery) {
    // Search Bible verse index for direct verse lookup
    String verseIndexPath = "/the_book/religious/bible_verse_index.txt";
    std::vector<SearchResult> verseResults = searchIndex(verseIndexPath, query);

    for (size_t j = 0; j < verseResults.size() && searchResults.size() < MAX_TOTAL_RESULTS; j++) {
      verseResults[j].category = "Bible";  // Tag as Bible verse
      searchResults.push_back(verseResults[j]);
    }

    Serial.printf("Scripture search found %d verses\n", searchResults.size());
  }

  // Standard search across all categories (if not enough results from verse search)
  for (int i = 0; i < totalCategories && searchResults.size() < MAX_TOTAL_RESULTS; i++) {
    String indexPath = categories[i].path + "/index.txt";
    std::vector<SearchResult> catResults = searchIndex(indexPath, query);

    // Add results from this category (up to limit)
    for (size_t j = 0; j < catResults.size() && searchResults.size() < MAX_TOTAL_RESULTS; j++) {
      searchResults.push_back(catResults[j]);
    }
  }

  Serial.printf("Search found %d results (max %d)\n", searchResults.size(), MAX_TOTAL_RESULTS);

  bookState = BOOK_RESULTS;
  drawBookResults();
}

void loadArticle(SearchResult result) {
  articleLines.clear();
  articleScrollOffset = 0;
  currentArticleTitle = result.title;

  // Build full path - find matching category
  String fullPath = "";
  for (int i = 0; i < totalCategories; i++) {
    // Extract category name from path (e.g., "/the_book/wikipedia" -> "Wiki")
    String catPath = categories[i].path;
    String catName = categories[i].name;

    // Match the result category to the full path
    if ((result.category == "Wiki" && catPath.indexOf("wikipedia") >= 0) ||
        (result.category == "Code" && catPath.indexOf("programming") >= 0) ||
        (result.category == "Survival" && catPath.indexOf("survival") >= 0) ||
        (result.category == "Religious" && catPath.indexOf("religious") >= 0) ||
        (result.category == "Plants" && catPath.indexOf("edible") >= 0) ||
        (result.category == catName)) {
      fullPath = catPath + "/" + result.filename;
      break;
    }
  }

  // Fallback: try to guess from category name
  if (fullPath == "") {
    String categoryLower = result.category;
    categoryLower.toLowerCase();
    fullPath = "/the_book/" + categoryLower + "/" + result.filename;
  }

  // Check if file exists
  if (!SD.exists(fullPath)) {
    articleLines.push_back("Error: Article file not found");
    articleLines.push_back("");
    articleLines.push_back("Path: " + fullPath);
    bookState = BOOK_ARTICLE;
    drawBookArticle();
    return;
  }

  File articleFile = SD.open(fullPath, FILE_READ);
  if (!articleFile) {
    articleLines.push_back("Error: Could not open article");
    bookState = BOOK_ARTICLE;
    drawBookArticle();
    return;
  }

  // Seek to the line number if specified
  if (result.lineNumber > 0) {
    for (int i = 0; i < result.lineNumber && articleFile.available(); i++) {
      articleFile.readStringUntil('\n');
    }
  }

  // Read article content (limit total display lines to avoid memory issues)
  // For very large files like Quran (1MB+), limit to 300 lines max
  const int MAX_DISPLAY_LINES = 300;
  const int CHARS_PER_LINE = 19;  // Size 2 font fits ~19 chars
  int linesRead = 0;

  while (articleFile.available() && articleLines.size() < MAX_DISPLAY_LINES) {
    String line = articleFile.readStringUntil('\n');
    line.trim();

    // Handle long lines - split into multiple display lines
    while (line.length() > CHARS_PER_LINE && articleLines.size() < MAX_DISPLAY_LINES) {
      articleLines.push_back(line.substring(0, CHARS_PER_LINE));
      line = line.substring(CHARS_PER_LINE);
    }

    if ((line.length() > 0 || articleLines.size() > 0) && articleLines.size() < MAX_DISPLAY_LINES) {
      articleLines.push_back(line);
    }

    linesRead++;

    // Additional safety: break if we've read too many source lines
    if (linesRead > 200) break;
  }

  articleFile.close();

  if (articleLines.size() == 0) {
    articleLines.push_back("(Empty article)");
  }

  bookState = BOOK_ARTICLE;
  drawBookArticle();
}

void scrollArticleUp() {
  if (articleScrollOffset > 0) {
    articleScrollOffset--;
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 30);
    drawBookArticle();
  }
}

void scrollArticleDown() {
  if (articleScrollOffset < (int)articleLines.size() - LINES_PER_PAGE) {
    articleScrollOffset++;
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 30);
    drawBookArticle();
  }
}

void clearSearch() {
  searchInput = "";
  searchResults.clear();
  selectedResultIndex = 0;
}
