#!/usr/bin/env python3
"""
BOOK TABLE OF CONTENTS GENERATOR
Creates a comprehensive TOC file for The Book
Shows what's available, organized by category with counts
"""

import os
from pathlib import Path
from collections import defaultdict

BOOK_PATH = Path("/Users/jamesfauntleroy/Documents/PlatformIO/Projects/LaboratoryM5/the_book")
MAX_LINE_WIDTH = 39

def wrap_text(text, width=MAX_LINE_WIDTH):
    """Wrap text to display width"""
    lines = []
    words = text.split()
    current_line = ""

    for word in words:
        test_line = current_line + " " + word if current_line else word
        if len(test_line) <= width:
            current_line = test_line
        else:
            if current_line:
                lines.append(current_line)
            current_line = word

    if current_line:
        lines.append(current_line)

    return '\n'.join(lines)

def scan_book_structure():
    """Scan the book directory structure"""
    structure = defaultdict(lambda: defaultdict(list))
    total_files = 0
    total_size = 0

    for root, dirs, files in os.walk(BOOK_PATH):
        root_path = Path(root)

        # Skip if this is the root
        if root_path == BOOK_PATH:
            continue

        for file in files:
            if file.endswith('.txt') and file != 'index.txt' and not file.startswith('_'):
                file_path = root_path / file
                relative_path = file_path.relative_to(BOOK_PATH)

                # Get category (top-level folder)
                parts = relative_path.parts
                if len(parts) >= 1:
                    category = parts[0]
                    subcategory = parts[1] if len(parts) > 2 else None

                    # Get file stats
                    try:
                        size = file_path.stat().st_size
                        total_size += size

                        # Extract title
                        title = file.replace('.txt', '').replace('_', ' ').title()

                        structure[category][subcategory].append({
                            'title': title,
                            'path': str(relative_path),
                            'size': size
                        })

                        total_files += 1
                    except:
                        pass

    return structure, total_files, total_size

def generate_toc():
    """Generate comprehensive TOC"""
    print("Scanning The Book directory...")

    structure, total_files, total_size = scan_book_structure()

    print(f"Found {total_files} articles ({total_size/(1024*1024):.1f} MB)")

    # Category descriptions
    descriptions = {
        "agriculture": "Farming, crops, animals, soil health",
        "diy_technical": "Tools, repairs, technical guides",
        "edible_plants": "Safe plants to eat in the wild",
        "history": "World history, wars, civilizations",
        "literature": "Classic books, poetry, philosophy",
        "medical": "Emergency care, diseases, first aid",
        "poisonous_plants": "Dangerous plants to avoid",
        "programming": "Code tutorials, algorithms, frameworks",
        "programming_languages": "Language references",
        "reference": "I Ching, dream symbols, guides",
        "religious": "Bible, Quran, spiritual texts",
        "science": "Physics, chemistry, biology, astronomy",
        "survival": "Emergency skills, water, fire, shelter",
        "tech": "Technology and computing",
        "wikipedia": "General knowledge encyclopedia"
    }

    # Create TOC content
    toc_lines = []

    # Header
    toc_lines.append("="*39)
    toc_lines.append("THE BOOK - TABLE OF CONTENTS")
    toc_lines.append("="*39)
    toc_lines.append("")
    toc_lines.append(f"Total Articles: {total_files:,}")
    toc_lines.append(f"Total Size: {total_size/(1024*1024):.1f} MB")
    toc_lines.append("")
    toc_lines.append("KNOWLEDGE CATEGORIES")
    toc_lines.append("-"*39)
    toc_lines.append("")

    # Sort categories alphabetically
    sorted_categories = sorted(structure.keys())

    # Category summaries
    for category in sorted_categories:
        subcats = structure[category]
        article_count = sum(len(articles) for articles in subcats.values())

        toc_lines.append(f"[{category.upper()}]")

        # Description
        desc = descriptions.get(category, "Knowledge collection")
        wrapped_desc = wrap_text(desc, 37)
        for line in wrapped_desc.split('\n'):
            toc_lines.append(f"  {line}")

        toc_lines.append(f"  Articles: {article_count}")

        # Subcategories if any
        non_none_subcats = [s for s in subcats.keys() if s is not None]
        if non_none_subcats:
            toc_lines.append("  Topics:")
            for subcat in sorted(non_none_subcats):
                count = len(subcats[subcat])
                subcat_name = subcat.replace('_', ' ').title()
                toc_lines.append(f"    - {subcat_name}: {count}")

        toc_lines.append("")

    # Detailed listing
    toc_lines.append("="*39)
    toc_lines.append("DETAILED ARTICLE LIST")
    toc_lines.append("="*39)
    toc_lines.append("")

    for category in sorted_categories:
        toc_lines.append(f"\n{'='*39}")
        toc_lines.append(f"{category.upper().replace('_', ' ')}")
        toc_lines.append(f"{'='*39}\n")

        subcats = structure[category]

        # Group by subcategory
        for subcat in sorted(subcats.keys()):
            articles = sorted(subcats[subcat], key=lambda x: x['title'])

            if subcat:
                toc_lines.append(f"\n{subcat.replace('_', ' ').title()}:")
                toc_lines.append("-"*39)

            for article in articles:
                # Truncate long titles
                title = article['title']
                if len(title) > 35:
                    title = title[:32] + "..."
                toc_lines.append(f"  • {title}")

            if articles:
                toc_lines.append("")

    # Footer
    toc_lines.append("\n" + "="*39)
    toc_lines.append("END OF TABLE OF CONTENTS")
    toc_lines.append("="*39)
    toc_lines.append("")
    toc_lines.append("Navigate to any category in The Book")
    toc_lines.append("to browse and read articles.")
    toc_lines.append("")
    toc_lines.append("Knowledge is power.")
    toc_lines.append("="*39)

    return '\n'.join(toc_lines)

def main():
    """Generate and save TOC"""
    print("="*70)
    print("BOOK TABLE OF CONTENTS GENERATOR".center(70))
    print("="*70)
    print()

    toc_content = generate_toc()

    # Save to book root
    toc_path = BOOK_PATH / "TOC.txt"
    with open(toc_path, 'w', encoding='utf-8') as f:
        f.write(toc_content)

    print(f"\n✅ Table of Contents created!")
    print(f"📄 Saved to: {toc_path}")
    print(f"📏 Size: {len(toc_content)/1024:.1f} KB")
    print("\nYou can now browse this file on your M5Cardputer")
    print("to see everything available in The Book!")

if __name__ == "__main__":
    main()
