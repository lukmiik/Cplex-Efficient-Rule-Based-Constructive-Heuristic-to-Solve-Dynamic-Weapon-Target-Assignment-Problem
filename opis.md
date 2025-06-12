# Opis projektu dynamic_target_weapon

Projekt **dynamic_target_weapon** to zaawansowany system do rozwiązywania dynamicznego problemu przydziału broni do celów (Dynamic Weapon-Target Assignment, DWTA). Jego celem jest optymalizacja ochrony zasobów poprzez efektywne przypisanie dostępnych środków bojowych do pojawiających się i zmieniających pozycję celów w kolejnych etapach działań. Projekt umożliwia porównanie dwóch podejść: szybkiej heurystyki regułowej oraz dokładnego modelu optymalizacyjnego wykorzystującego solver CPLEX.

## Opis problemu DWTA

Dynamiczny problem przydziału broni do celów polega na sekwencyjnym przypisywaniu dostępnych środków bojowych do wykrytych celów w taki sposób, by zminimalizować oczekiwane straty własnych zasobów lub zmaksymalizować ich przetrwanie. Problem ten jest złożony obliczeniowo, ponieważ każda decyzja wpływa na kolejne etapy (np. przez zmiany w liczbie dostępnych broni i przetrwaniu celów), a liczba możliwych kombinacji rośnie wykładniczo wraz z rozmiarem instancji.

## Architektura i działanie programu

Projekt składa się z kilku kluczowych modułów:

- **Moduł heurystyki regułowej**: Implementuje szybki algorytm konstrukcyjny, który w każdym etapie wybiera przypisania broni do celów na podstawie wartości heurystycznej (VQP), obliczanej jako iloczyn wartości zasobu, prawdopodobieństwa zniszczenia celu i aktualnego prawdopodobieństwa jego przetrwania. Po każdej decyzji system aktualizuje stany i ponownie ocenia możliwe przypisania.
- **Moduł optymalizacyjny CPLEX**: Formułuje problem jako mieszane programowanie całkowitoliczbowe (MILP). Zmienna decyzyjna oznacza przypisanie broni do celu w danym etapie. Oryginalna funkcja celu, będąca nieliniowa (multiplikatywna), została **zlinearyzowana** – prawdopodobieństwa przetrwania celów i zasobów są aproksymowane sumami, co umożliwia rozwiązanie problemu przez CPLEX.
- **Moduły zarządzania bronią, celami i zasobami**: Odpowiadają za śledzenie stanu wszystkich elementów systemu w kolejnych etapach.

## Modyfikacja funkcji celu w CPLEX

W modelu optymalizacyjnym CPLEX funkcja celu została **nieznacznie zmieniona w sposób liniowy**. Oryginalnie prawdopodobieństwo przetrwania celu i zasobu wyraża się jako iloczyn (nieliniowość), natomiast w modelu CPLEX przyjęto liniową aproksymację:  
- Prawdopodobieństwo przetrwania celu: `1 - SUM(weaponLethality[i][j][t] * x[i][j][t])`
- Prawdopodobieństwo przetrwania zasobu: `1 - SUM(targetLethality[j][k] * targetSurvival[j])`  
Dzięki temu cała funkcja celu jest liniowa względem zmiennych decyzyjnych, co pozwala na jej efektywne rozwiązanie przez solver MILP.

## Wyniki eksperymentów

### Część 1: Dane wejściowe

| Instancja             | Liczba broni (W) | Liczba celów (T) | Liczba zasobów (K) | Liczba etapów (S) | Wartość aktywów (całk.) |
|-----------------------|------------------|------------------|--------------------|-------------------|-------------------------|
| output100100104-3.txt | 100              | 100              | 10                 | 4                 | 590.032                 |
| output150100104.txt   | 150              | 100              | 10                 | 4                 | 513.585                 |
| output100150104-2.txt | 100              | 150              | 10                 | 4                 | 516.811                 |

### Część 2: Wyniki i czas działania

| Instancja             | Heurystyka: wartość przetrwała | Heurystyka: % przetrwania | Heurystyka: czas [s] | CPLEX: wartość przetrwała | CPLEX: % przetrwania | CPLEX: czas [s] | Różnica jakości |
|-----------------------|-------------------------------|--------------------------|----------------------|---------------------------|---------------------|------------------|-----------------|
| output100100104-3.txt | 549.993                       | 93.21%                   | 0.104                | 582.227                   | 98.68%              | 60.09            | +5.47%          |
| output150100104.txt   | 507.710                       | 98.86%                   | 0.207                | 510.270                   | 99.35%              | 60.17            | +0.49%          |
| output100150104-2.txt | 353.539                       | 68.41%                   | 0.210                | 353.539                   | 68.41%              | 60.11            | 0.00%           |

## Analiza

- **Jakość rozwiązań:** CPLEX uzyskuje zawsze co najmniej tak dobry wynik jak heurystyka, a często wyraźnie lepszy (do +5.47 punktów procentowych). W najtrudniejszym przypadku (100 broni, 150 celów) obie metody osiągnęły identyczny wynik, co sugeruje, że heurystyka może być bardzo efektywna dla wybranych instancji.
- **Czas działania:** Heurystyka działa błyskawicznie (0.1–0.2 s), podczas gdy CPLEX wymaga pełnej minuty obliczeń, nawet z warm startem.
- **Skalowalność:** Wraz ze wzrostem rozmiaru problemu różnica jakości maleje, a przewaga heurystyki w czasie działania pozostaje niezmienna.

## Podsumowanie

Projekt dynamic_target_weapon demonstruje praktyczne zastosowanie zarówno szybkich heurystyk, jak i zaawansowanych metod optymalizacyjnych do rozwiązywania dynamicznych problemów przydziału broni do celów.  
- **Heurystyka regułowa** sprawdza się tam, gdzie liczy się czas reakcji i akceptowalna jest niewielka utrata jakości.
- **Model optymalizacyjny CPLEX** pozwala na osiągnięcie rozwiązań bliskich optimum, lecz wymaga znacznie więcej czasu obliczeniowego.

Projekt umożliwia więc analizę kompromisu między jakością ochrony zasobów a czasem reakcji systemu, co jest kluczowe w zastosowaniach militarnych i symulacyjnych.
