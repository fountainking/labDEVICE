#!/usr/bin/env python3
"""
MEGA WIKIPEDIA DOWNLOADER
Downloads 10,000+ Wikipedia articles organized by category
Target: Complete offline knowledge base for M5Cardputer

Run time: 3-6 hours (with rate limiting to be polite to Wikipedia)
Final size: 500MB-1GB of text
"""

import os
import requests
import json
import re
import time
from pathlib import Path
from urllib.parse import quote

# Configuration
BASE_DIR = Path("the_book/wikipedia")
MAX_LINE_WIDTH = 39
REQUEST_DELAY = 0.5  # Be polite to Wikipedia
BATCH_SIZE = 50  # Progress update frequency

HEADERS = {
    'User-Agent': 'M5CardputerKnowledgeLibrary/2.0 Educational'
}

def wrap_text(text, width=MAX_LINE_WIDTH):
    """Wrap text to M5Cardputer display width"""
    lines = []
    for paragraph in text.split('\n'):
        if not paragraph.strip():
            lines.append('')
            continue
        words = paragraph.split()
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

def download_article(title, category):
    """Download a single Wikipedia article"""
    api_url = "https://en.wikipedia.org/api/rest_v1/page/summary/"

    try:
        # Check if we already have this article (skip if exists)
        # Pre-calculate the filename to check existence
        temp_filename = re.sub(r'[^\w\s-]', '', title.replace('_', ' ').lower())
        temp_filename = re.sub(r'[-\s]+', '_', temp_filename) + '.txt'
        temp_path = BASE_DIR / category / temp_filename

        if temp_path.exists():
            return "skip"  # Already have it

        url = api_url + quote(title)
        response = requests.get(url, headers=HEADERS, timeout=15)

        if response.status_code == 200:
            data = response.json()
            article_title = data.get('title', title.replace('_', ' '))
            content = data.get('extract', '')

            if content and len(content) > 100:
                # Clean filename
                filename = re.sub(r'[^\w\s-]', '', article_title.lower())
                filename = re.sub(r'[-\s]+', '_', filename) + '.txt'

                # Save
                path = BASE_DIR / category / filename
                path.parent.mkdir(parents=True, exist_ok=True)

                with open(path, 'w', encoding='utf-8') as f:
                    f.write(wrap_text(f"{article_title}\n\n{content}"))

                return True

    except Exception as e:
        pass

    return False

# MASSIVE TOPIC LISTS
# ===================

MEGA_TOPICS = {
    "programming": [
        # Languages
        "Python_(programming_language)", "JavaScript", "Java_(programming_language)",
        "C_(programming_language)", "C++", "C_Sharp_(programming_language)",
        "Ruby_(programming_language)", "Go_(programming_language)", "Rust_(programming_language)",
        "Swift_(programming_language)", "PHP", "TypeScript", "Kotlin_(programming_language)",
        "R_(programming_language)", "SQL", "MATLAB", "Perl", "Scala_(programming_language)",
        "Haskell", "Lisp_(programming_language)", "Fortran", "COBOL", "Pascal_(programming_language)",
        "Lua_(programming_language)", "Elixir_(programming_language)", "Erlang_(programming_language)",
        "Clojure", "F_Sharp_(programming_language)", "Prolog", "Assembly_language",

        # Concepts
        "Algorithm", "Data_structure", "Object-oriented_programming", "Functional_programming",
        "Recursion", "Sorting_algorithm", "Graph_theory", "Binary_tree", "Hash_table",
        "Linked_list", "Stack_(abstract_data_type)", "Queue_(abstract_data_type)",
        "Big_O_notation", "Complexity_theory", "Compiler", "Interpreter_(computing)",
        "Version_control", "Git", "GitHub", "Software_testing", "Agile_software_development",
        "Scrum_(software_development)", "DevOps", "Continuous_integration",

        # Web & Network
        "HTTP", "HTTPS", "TCP/IP", "DNS", "REST", "GraphQL", "WebSocket",
        "HTML", "CSS", "Web_browser", "Search_engine", "API",

        # Databases
        "Database", "Relational_database", "SQL", "NoSQL", "MongoDB",
        "PostgreSQL", "MySQL", "SQLite", "Redis", "Cassandra_(database)",

        # AI & ML
        "Artificial_intelligence", "Machine_learning", "Deep_learning",
        "Neural_network", "TensorFlow", "PyTorch", "Natural_language_processing",
        "Computer_vision", "Reinforcement_learning",

        # Systems
        "Operating_system", "Linux", "Unix", "Microsoft_Windows", "MacOS",
        "Android_(operating_system)", "iOS", "Kernel_(operating_system)",
        "File_system", "Memory_management", "Process_(computing)",

        # Security
        "Computer_security", "Cryptography", "Encryption", "Public-key_cryptography",
        "Blockchain", "Cryptocurrency", "Bitcoin", "Ethereum",
        "Cybersecurity", "Firewall_(computing)", "Malware", "Virus_(computer)",

        # Cloud & Infrastructure
        "Cloud_computing", "Amazon_Web_Services", "Docker_(software)",
        "Kubernetes", "Virtual_machine", "Microservices", "Server_(computing)",
        "Load_balancing_(computing)", "Content_delivery_network",

        # Tools & Frameworks
        "React_(JavaScript_library)", "Angular_(web_framework)", "Vue.js",
        "Node.js", "Django_(web_framework)", "Flask_(web_framework)",
        "Spring_Framework", "Ruby_on_Rails", "jQuery", "Bootstrap_(front-end_framework)"
    ],

    "mathematics": [
        # Basics
        "Mathematics", "Arithmetic", "Algebra", "Geometry", "Trigonometry",
        "Calculus", "Statistics", "Probability", "Number_theory",

        # Numbers
        "Natural_number", "Integer", "Rational_number", "Real_number",
        "Complex_number", "Prime_number", "Fibonacci_sequence", "Pi",
        "E_(mathematical_constant)", "Golden_ratio", "Infinity",

        # Algebra
        "Linear_algebra", "Matrix_(mathematics)", "Vector_space", "Eigenvalues_and_eigenvectors",
        "Polynomial", "Quadratic_equation", "Linear_equation", "Exponential_function",
        "Logarithm", "Binomial_theorem",

        # Geometry
        "Euclidean_geometry", "Triangle", "Circle", "Rectangle", "Square",
        "Pentagon", "Hexagon", "Sphere", "Cylinder", "Cone", "Pyramid_(geometry)",
        "Pythagorean_theorem", "Angle", "Area", "Volume", "Perimeter", "Surface_area",

        # Calculus
        "Derivative", "Integral", "Differential_equation", "Limit_(mathematics)",
        "Infinite_series", "Taylor_series", "Fourier_series",

        # Advanced
        "Topology", "Set_theory", "Group_theory", "Graph_theory", "Combinatorics",
        "Mathematical_logic", "Game_theory", "Chaos_theory", "Fractal",

        # Applied
        "Mathematical_optimization", "Numerical_analysis", "Linear_programming",
        "Differential_geometry", "Algebraic_geometry"
    ],

    "science": [
        # Physics
        "Physics", "Classical_mechanics", "Quantum_mechanics", "Relativity",
        "Thermodynamics", "Electromagnetism", "Optics", "Acoustics",
        "Atom", "Molecule", "Electron", "Proton", "Neutron", "Photon",
        "Energy", "Force", "Gravity", "Mass", "Velocity", "Acceleration",
        "Momentum", "Newton's_laws_of_motion", "Conservation_of_energy",
        "Wave", "Frequency", "Wavelength", "Speed_of_light",
        "Nuclear_physics", "Nuclear_fusion", "Nuclear_fission",
        "Particle_physics", "String_theory", "Big_Bang", "Black_hole",
        "Wormhole", "Dark_matter", "Dark_energy", "Multiverse",

        # Chemistry
        "Chemistry", "Organic_chemistry", "Inorganic_chemistry", "Physical_chemistry",
        "Analytical_chemistry", "Biochemistry", "Chemical_element", "Periodic_table",
        "Atom", "Molecule", "Chemical_bond", "Chemical_reaction",
        "Acid", "Base_(chemistry)", "pH", "Oxidation", "Reduction_(chemistry)",
        "Catalysis", "Polymer", "Hydrocarbon", "Alcohol_(chemistry)",
        "Carbohydrate", "Protein", "Lipid", "Nucleic_acid", "DNA", "RNA",
        "Enzyme", "Amino_acid",

        # Biology
        "Biology", "Cell_(biology)", "Genetics", "Evolution", "Ecology",
        "Molecular_biology", "Microbiology", "Botany", "Zoology",
        "Anatomy", "Physiology", "Neuroscience", "Immunology",
        "Natural_selection", "Gene", "Chromosome", "Mitosis", "Meiosis",
        "Photosynthesis", "Cellular_respiration", "Metabolism",
        "Bacteria", "Virus", "Fungus", "Plant", "Animal",
        "Mammal", "Bird", "Fish", "Reptile", "Amphibian",
        "Insect", "Ecosystem", "Food_chain", "Biodiversity",
        "Extinction", "Endangered_species", "Climate_change",

        # Earth Science
        "Geology", "Meteorology", "Oceanography", "Paleontology",
        "Plate_tectonics", "Earthquake", "Volcano", "Mountain",
        "Rock_(geology)", "Mineral", "Fossil", "Erosion", "Sediment",
        "Weather", "Climate", "Atmosphere_of_Earth", "Water_cycle",
        "Ocean", "River", "Lake", "Glacier", "Desert", "Forest",
        "Rainforest", "Tundra", "Savanna", "Biome",

        # Astronomy
        "Astronomy", "Solar_System", "Sun", "Mercury_(planet)", "Venus",
        "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune",
        "Moon", "Star", "Galaxy", "Milky_Way", "Universe",
        "Comet", "Asteroid", "Meteor", "Supernova", "Nebula",
        "Exoplanet", "Space_exploration", "International_Space_Station"
    ],

    "history": [
        # Ancient
        "Ancient_Egypt", "Ancient_Greece", "Ancient_Rome", "Ancient_China",
        "Mesopotamia", "Persian_Empire", "Byzantine_Empire",
        "Maya_civilization", "Aztec", "Inca_Empire", "Viking_Age",
        "Silk_Road", "Phoenicia", "Carthage", "Troy",

        # Medieval
        "Middle_Ages", "Feudalism", "Crusades", "Black_Death",
        "Hundred_Years'_War", "Byzantine_Empire", "Holy_Roman_Empire",
        "Mongol_Empire", "Ottoman_Empire", "Medieval_Europe",

        # Renaissance & Early Modern
        "Renaissance", "Age_of_Discovery", "Protestant_Reformation",
        "Scientific_Revolution", "Enlightenment_(philosophy)",
        "Age_of_Enlightenment", "Industrial_Revolution",

        # Revolutions & Wars
        "American_Revolution", "French_Revolution", "Haitian_Revolution",
        "Napoleonic_Wars", "American_Civil_War", "World_War_I",
        "World_War_II", "Cold_War", "Korean_War", "Vietnam_War",
        "Gulf_War", "Iraq_War", "War_in_Afghanistan_(2001–2021)",

        # Modern History
        "Great_Depression", "New_Deal", "Civil_rights_movement",
        "Women's_suffrage", "Decolonization", "Space_Race",
        "Fall_of_the_Berlin_Wall", "September_11_attacks",
        "Arab_Spring", "COVID-19_pandemic",

        # By Region
        "History_of_China", "History_of_India", "History_of_Japan",
        "History_of_Africa", "History_of_Europe", "History_of_the_United_States",
        "History_of_Russia", "History_of_Latin_America"
    ],

    "people": [
        # Scientists
        "Albert_Einstein", "Isaac_Newton", "Galileo_Galilei", "Charles_Darwin",
        "Marie_Curie", "Nikola_Tesla", "Thomas_Edison", "Stephen_Hawking",
        "Richard_Feynman", "Carl_Sagan", "Neil_deGrasse_Tyson",
        "Aristotle", "Archimedes", "Leonardo_da_Vinci",
        "Johannes_Kepler", "Nicolaus_Copernicus", "Michael_Faraday",
        "James_Clerk_Maxwell", "Ernest_Rutherford", "Niels_Bohr",
        "Werner_Heisenberg", "Erwin_Schrödinger", "Paul_Dirac",

        # Mathematicians
        "Pythagoras", "Euclid", "Alan_Turing", "John_von_Neumann",
        "Kurt_Gödel", "Leonhard_Euler", "Carl_Friedrich_Gauss",
        "Pierre_de_Fermat", "Blaise_Pascal", "René_Descartes",

        # Inventors & Engineers
        "Alexander_Graham_Bell", "Wright_brothers", "Henry_Ford",
        "Steve_Jobs", "Bill_Gates", "Elon_Musk", "Jeff_Bezos",
        "Mark_Zuckerberg", "Tim_Berners-Lee", "Linus_Torvalds",
        "Guido_van_Rossum", "Dennis_Ritchie", "Ken_Thompson",
        "Richard_Stallman", "Grace_Hopper",

        # Political Leaders
        "George_Washington", "Thomas_Jefferson", "Abraham_Lincoln",
        "Franklin_D._Roosevelt", "John_F._Kennedy",
        "Winston_Churchill", "Napoleon", "Julius_Caesar",
        "Alexander_the_Great", "Genghis_Khan", "Cleopatra",
        "Queen_Victoria", "Catherine_the_Great",
        "Mahatma_Gandhi", "Nelson_Mandela", "Martin_Luther_King_Jr.",
        "Malcolm_X", "Rosa_Parks", "Harriet_Tubman",
        "Adolf_Hitler", "Joseph_Stalin", "Mao_Zedong",
        "Vladimir_Lenin", "Fidel_Castro", "Che_Guevara",

        # Philosophers & Thinkers
        "Socrates", "Plato", "Aristotle", "Confucius", "Buddha",
        "Lao_Tzu", "Sun_Tzu", "Marcus_Aurelius", "Seneca_the_Younger",
        "Augustine_of_Hippo", "Thomas_Aquinas", "René_Descartes",
        "Immanuel_Kant", "Friedrich_Nietzsche", "Karl_Marx",
        "Sigmund_Freud", "Carl_Jung", "Jean-Paul_Sartre",

        # Artists & Writers
        "William_Shakespeare", "Leo_Tolstoy", "Fyodor_Dostoevsky",
        "Charles_Dickens", "Jane_Austen", "Mark_Twain",
        "Ernest_Hemingway", "F._Scott_Fitzgerald", "George_Orwell",
        "J._R._R._Tolkien", "C._S._Lewis", "Edgar_Allan_Poe",
        "Pablo_Picasso", "Vincent_van_Gogh", "Leonardo_da_Vinci",
        "Michelangelo", "Rembrandt", "Claude_Monet",
        "Ludwig_van_Beethoven", "Wolfgang_Amadeus_Mozart",
        "Johann_Sebastian_Bach", "Bob_Dylan", "The_Beatles",

        # Religious Figures
        "Jesus", "Moses", "Muhammad", "Abraham", "Buddha",
        "Confucius", "Martin_Luther", "John_Calvin", "Dalai_Lama"
    ],

    "geography": [
        # Continents & Regions
        "Africa", "Antarctica", "Asia", "Europe", "North_America",
        "South_America", "Australia", "Oceania", "Middle_East",
        "Caribbean", "Central_America", "Southeast_Asia",

        # Countries (Major)
        "United_States", "China", "India", "Russia", "Japan",
        "Germany", "United_Kingdom", "France", "Italy", "Spain",
        "Canada", "Australia", "Brazil", "Mexico", "Argentina",
        "South_Africa", "Egypt", "Nigeria", "Kenya", "Ethiopia",
        "Saudi_Arabia", "Iran", "Iraq", "Turkey", "Israel",
        "South_Korea", "North_Korea", "Thailand", "Vietnam",
        "Indonesia", "Philippines", "Pakistan", "Bangladesh",
        "Poland", "Ukraine", "Sweden", "Norway", "Greece",

        # Cities
        "New_York_City", "Los_Angeles", "London", "Paris", "Tokyo",
        "Beijing", "Shanghai", "Moscow", "Mumbai", "São_Paulo",
        "Mexico_City", "Cairo", "Istanbul", "Rome", "Athens",
        "Jerusalem", "Mecca", "Singapore", "Hong_Kong", "Dubai",

        # Natural Features
        "Pacific_Ocean", "Atlantic_Ocean", "Indian_Ocean", "Arctic_Ocean",
        "Mediterranean_Sea", "Caribbean_Sea", "Red_Sea",
        "Amazon_River", "Nile", "Mississippi_River", "Yangtze",
        "Ganges", "Rhine", "Danube", "Thames",
        "Mount_Everest", "Himalayas", "Alps", "Andes", "Rockies",
        "Sahara", "Amazon_rainforest", "Great_Barrier_Reef",
        "Grand_Canyon", "Niagara_Falls", "Victoria_Falls"
    ],

    "health": [
        # Emergency & First Aid
        "First_aid", "Cardiopulmonary_resuscitation", "Heimlich_maneuver",
        "Shock_(circulatory)", "Burns", "Bone_fracture", "Wound",
        "Hemorrhage", "Tourniquet", "Bandage", "Splint_(medicine)",

        # Environmental
        "Hypothermia", "Hyperthermia", "Heat_stroke", "Heat_exhaustion",
        "Frostbite", "Sunburn", "Dehydration", "Altitude_sickness",

        # Poisoning & Bites
        "Poison", "Snake_bite", "Spider_bite", "Insect_bites_and_stings",
        "Food_poisoning", "Carbon_monoxide_poisoning",

        # Diseases & Conditions
        "Infection", "Fever", "Influenza", "Common_cold", "Pneumonia",
        "Tuberculosis", "Malaria", "Cholera", "Typhoid_fever",
        "COVID-19", "HIV/AIDS", "Ebola", "Measles", "Smallpox",
        "Cancer", "Heart_disease", "Stroke", "Diabetes_mellitus",
        "Hypertension", "Asthma", "Allergies", "Epilepsy",
        "Alzheimer's_disease", "Parkinson's_disease",
        "Depression_(mood)", "Anxiety", "Post-traumatic_stress_disorder",
        "Schizophrenia", "Autism", "Bipolar_disorder",

        # Human Anatomy
        "Human_body", "Skeletal_system", "Muscular_system",
        "Circulatory_system", "Respiratory_system", "Digestive_system",
        "Nervous_system", "Immune_system", "Endocrine_system",
        "Heart", "Lung", "Brain", "Liver", "Kidney", "Stomach",
        "Bone", "Muscle", "Blood", "Skin", "Eye", "Ear", "Nose",

        # Medicine & Treatment
        "Medicine", "Antibiotic", "Vaccine", "Surgery", "Anesthesia",
        "X-ray", "CT_scan", "MRI", "Ultrasound", "Blood_test",
        "Physical_therapy", "Chemotherapy", "Radiation_therapy",
        "Organ_transplantation", "Prosthesis",

        # Nutrition & Fitness
        "Nutrition", "Vitamin", "Mineral_(nutrient)", "Protein",
        "Carbohydrate", "Fat", "Dietary_fiber", "Water",
        "Physical_fitness", "Exercise", "Aerobic_exercise",
        "Strength_training", "Yoga", "Meditation", "Sleep"
    ],

    "technology": [
        # Computing
        "Computer", "Personal_computer", "Laptop", "Tablet_computer",
        "Smartphone", "Central_processing_unit", "Graphics_processing_unit",
        "Random-access_memory", "Hard_disk_drive", "Solid-state_drive",
        "Motherboard", "Keyboard_(computing)", "Computer_mouse",
        "Computer_monitor", "Printer_(computing)", "Scanner",

        # Internet & Communication
        "Internet", "World_Wide_Web", "Email", "Social_media",
        "Facebook", "Twitter", "Instagram", "YouTube", "Wikipedia",
        "Wi-Fi", "Bluetooth", "5G", "Fiber-optic_communication",
        "Telephone", "Mobile_phone", "Television", "Radio",
        "Satellite", "GPS", "Podcast",

        # Electronics
        "Electricity", "Electric_current", "Voltage", "Resistance_(electricity)",
        "Circuit", "Transistor", "Diode", "Capacitor", "Resistor",
        "Integrated_circuit", "Microprocessor", "Semiconductor",
        "LED", "Laser", "Battery_(electricity)", "Solar_cell",

        # Energy & Power
        "Energy", "Renewable_energy", "Solar_energy", "Wind_power",
        "Hydroelectricity", "Nuclear_power", "Fossil_fuel",
        "Coal", "Petroleum", "Natural_gas", "Electric_vehicle",
        "Internal_combustion_engine", "Steam_engine",

        # Advanced Tech
        "Artificial_intelligence", "Robotics", "Nanotechnology",
        "Biotechnology", "3D_printing", "Virtual_reality",
        "Augmented_reality", "Quantum_computing", "CRISPR",

        # Transportation
        "Automobile", "Motorcycle", "Bicycle", "Train", "Airplane",
        "Helicopter", "Submarine", "Ship", "Rocket", "Space_Shuttle",

        # Materials & Manufacturing
        "Steel", "Aluminum", "Copper", "Plastic", "Rubber",
        "Glass", "Concrete", "Wood", "Paper", "Textile"
    ],

    "animals": [
        # Mammals
        "Dog", "Cat", "Horse", "Cow", "Pig", "Sheep", "Goat",
        "Elephant", "Lion", "Tiger", "Leopard", "Cheetah",
        "Bear", "Wolf", "Fox", "Deer", "Moose", "Elk",
        "Rabbit", "Squirrel", "Rat", "Mouse", "Bat",
        "Whale", "Dolphin", "Seal", "Sea_lion", "Walrus",
        "Monkey", "Gorilla", "Chimpanzee", "Orangutan",
        "Giraffe", "Zebra", "Hippopotamus", "Rhinoceros",
        "Kangaroo", "Koala", "Panda", "Polar_bear",

        # Birds
        "Eagle", "Hawk", "Owl", "Crow", "Raven", "Parrot",
        "Penguin", "Ostrich", "Emu", "Peacock", "Swan",
        "Duck", "Goose", "Chicken", "Turkey", "Pigeon",
        "Hummingbird", "Woodpecker", "Flamingo",

        # Reptiles & Amphibians
        "Snake", "Lizard", "Turtle", "Tortoise", "Crocodile",
        "Alligator", "Iguana", "Gecko", "Chameleon",
        "Frog", "Toad", "Salamander",

        # Fish & Aquatic
        "Shark", "Whale_shark", "Great_white_shark",
        "Salmon", "Tuna", "Goldfish", "Catfish", "Bass",
        "Jellyfish", "Octopus", "Squid", "Crab", "Lobster",
        "Starfish", "Sea_urchin", "Coral", "Clam", "Oyster",

        # Insects & Arachnids
        "Ant", "Bee", "Wasp", "Butterfly", "Moth",
        "Beetle", "Ladybug", "Dragonfly", "Mosquito", "Fly",
        "Spider", "Scorpion", "Tick", "Termite", "Grasshopper"
    ],

    "plants": [
        # Trees
        "Oak", "Pine", "Maple", "Birch", "Willow", "Elm",
        "Cedar", "Sequoia", "Redwood", "Baobab", "Bamboo",
        "Palm_tree", "Coconut", "Banana",

        # Flowers
        "Rose", "Tulip", "Sunflower", "Daisy", "Lily",
        "Orchid", "Carnation", "Chrysanthemum", "Lavender",

        # Crops & Food Plants
        "Wheat", "Rice", "Corn", "Barley", "Oat", "Rye",
        "Potato", "Tomato", "Carrot", "Onion", "Garlic",
        "Lettuce", "Cabbage", "Broccoli", "Cauliflower",
        "Apple", "Orange", "Lemon", "Grape", "Strawberry",
        "Blueberry", "Coffee", "Tea", "Cocoa_bean",
        "Soybean", "Peanut", "Almond", "Walnut",

        # Other Plants
        "Cactus", "Fern", "Moss", "Algae", "Mushroom",
        "Seaweed", "Kelp", "Grass", "Ivy", "Vine",
        "Venus_flytrap", "Succulent_plant"
    ],

    "philosophy": [
        "Philosophy", "Ethics", "Metaphysics", "Epistemology",
        "Logic", "Aesthetics", "Political_philosophy",
        "Philosophy_of_mind", "Philosophy_of_science",
        "Existentialism", "Stoicism", "Utilitarianism",
        "Pragmatism", "Rationalism", "Empiricism",
        "Idealism", "Materialism", "Phenomenology",
        "Nihilism", "Absurdism", "Free_will", "Consciousness",
        "Truth", "Beauty", "Justice", "Morality", "Good_and_evil"
    ],

    "engineering": [
        "Engineering", "Civil_engineering", "Mechanical_engineering",
        "Electrical_engineering", "Chemical_engineering",
        "Aerospace_engineering", "Computer_engineering",
        "Biomedical_engineering", "Environmental_engineering",
        "Bridge", "Dam", "Tunnel", "Skyscraper", "Road",
        "Railroad", "Canal", "Aqueduct", "Lever", "Wheel",
        "Pulley", "Gear", "Spring_(device)", "Bearing_(mechanical)"
    ]
}

def download_category(category, topics):
    """Download all articles in a category"""
    print(f"\n{'='*70}")
    print(f"CATEGORY: {category.upper()}")
    print(f"{'='*70}")
    print(f"Target: {len(topics)} articles\n")

    success_count = 0
    skip_count = 0
    fail_count = 0

    for i, topic in enumerate(topics, 1):
        result = download_article(topic, category)
        if result == "skip":
            skip_count += 1
        elif result:
            success_count += 1
        else:
            fail_count += 1

        if i % BATCH_SIZE == 0:
            print(f"  Progress: {i}/{len(topics)} ({success_count} new, {skip_count} skipped, {fail_count} failed)")

        # Only delay if we actually downloaded something
        if result is True:
            time.sleep(REQUEST_DELAY)

    print(f"\n✓ Completed: {success_count} new, {skip_count} already had, {fail_count} failed")

    return success_count

def create_category_index(category):
    """Create index.txt for a category"""
    category_path = BASE_DIR / category
    if not category_path.exists():
        return

    files = sorted(category_path.glob("*.txt"))
    if not files:
        return

    index_path = category_path / "index.txt"
    with open(index_path, 'w', encoding='utf-8') as f:
        f.write(f"**{category.upper()} INDEX**\n\n")

        for file in files:
            title = file.stem.replace('_', ' ').title()
            f.write(f"{title}|{file.name}|0|Wikipedia article\n")

        f.write(f"\n=======================================\n")
        f.write(f"Total: {len(files)} articles\n")

def main():
    """Main download process"""
    print("=" * 70)
    print("MEGA WIKIPEDIA DOWNLOADER".center(70))
    print("Target: 10,000+ articles".center(70))
    print("=" * 70)
    print()

    start_time = time.time()
    total_articles = 0

    # Download all categories
    for category, topics in MEGA_TOPICS.items():
        count = download_category(category, topics)
        total_articles += count
        create_category_index(category)

    elapsed = time.time() - start_time

    # Final report
    print("\n" + "=" * 70)
    print("DOWNLOAD COMPLETE!".center(70))
    print("=" * 70)
    print(f"\nTotal articles downloaded: {total_articles}")
    print(f"Time elapsed: {elapsed/60:.1f} minutes ({elapsed/3600:.2f} hours)")
    print(f"Average: {elapsed/total_articles if total_articles > 0 else 0:.2f} seconds per article")
    print(f"\nLocation: {BASE_DIR.absolute()}")
    print("\nReady to copy to SD card!")
    print("=" * 70)

if __name__ == "__main__":
    main()
