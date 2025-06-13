# dynamic_target_weapon

Projekt rozwiązujący problem dynamicznego przypisywania broni do celów (Dynamic Weapon-Target Assignment, DWTA) z wykorzystaniem podejścia heurystycznego oraz modelu optymalizacyjnego.

## Autorzy
Łukasz Łukaszewski
Paweł Kwieciński
Jakub Szafranek

---

## Struktura projektu
```
dynamic_target_weapon/
│
├── wyniki/ # folder z wynikami działania programu main.cpp
│ └── ... # pliki wynikowe generowane przez aplikację
│
├── main.cpp # główny plik źródłowy programu
├── opis.md # szczegółowy opis projektu
└── An_Efficient_Rule-Based_Constructive_Heuristic_to_Solve_Dynamic_Weapon-Target_Assignment_Problem.pdf # opis problemu jako podejście heurystyczne
```
## Opis plików i folderów

### wyniki/
W tym folderze znajdują się pliki wynikowe generowane przez program `main.cpp`. Przechowywane są tam dane dotyczące działania algorytmu heurystycznego oraz modelu optymalizacyjnego CPLEX.

### main.cpp
Główny plik źródłowy programu. Zawiera implementację algorytmu heurystycznego oraz modelu optymalizacyjnego do rozwiązywania problemu DWTA.

### opis.md
Szczegółowy opis projektu, jego funkcjonalności, architektury oraz wyników eksperymentów.

### Artykuł
Artykuł na bazie którego został wykonany projekt: https://www.researchgate.net/publication/224203320_An_Efficient_Rule-Based_Constructive_Heuristic_to_Solve_Dynamic_Weapon-Target_Assignment_Problem
