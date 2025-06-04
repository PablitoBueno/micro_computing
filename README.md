# micro_notebook
| P | Q | R | ¬Q | ¬P | ¬Q ∨ R ∨ ¬Q | P → R (¬P ∨ R) | LHS ((¬Q ∨ R ∨ ¬Q) ∧ (P → R)) | ¬P ∧ ¬Q | R ∨ (¬P ∧ ¬Q) | LHS ⇔ RHS |
|---|---|---|-----|-----|----------------|---------------------|-------------------------------|---------|------------------|-------------|
| F | F | F |  V  |  V  |       V        |          V          |              V                |    V    |        V         |      V      |
| F | F | V |  V  |  V  |       V        |          V          |              V                |    V    |        V         |      V      |
| F | V | F |  F  |  V  |       F        |          V          |              F                |    F    |        F         |      V      |
| F | V | V |  F  |  V  |       V        |          V          |              V                |    F    |        V         |      V      |
| V | F | F |  V  |  F  |       V        |          F          |              F                |    F    |        F         |      V      |
| V | F | V |  V  |  F  |       V        |          V          |              V                |    F    |        V         |      V      |
| V | V | F |  F  |  F  |       F        |          F          |              F                |    F    |        F         |      V      |
| V | V | V |  F  |  F  |       V        |          V          |              V                |    F    |        V         |      V      |
