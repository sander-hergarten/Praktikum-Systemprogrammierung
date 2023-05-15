Eine Scheduling Strategie legt fest welcher Prozess bei einem Prozesswechsel Rechenzeit erhalten soll. Einem [[Scheduling|Schedular]] stehen in der Regel mehrere Strategien zur Verfuegung

In Versuch 2 werden follgende Strategien behandelt:
- [Even](### EVEN)
- [Random](### RANDOM)
- [Run To Completion](### Run To Completion)
- [Round Robin](### Round Robin)
- [Inactive Aging](### Inactive Aging)

### EVEN
Die Even-Strategie durchläuft die auswählbaren Prozesse in aufsteigender Rei- henfolge, sodass jeder Prozess gleichmäßig oft ausgewählt wird und auf den letzten aus- wählbaren Prozess wieder der erste folgt.

### RANDOM
Die Random-Strategie wählt zufällig einen der auswählbaren Prozesse gemäß der diskreten Gleichverteilung in konstanter Laufzeit aus.

### Run To Completion
Die RunToCompletion-Strategie wählt den aktuell laufenden Pro- zess so lange aus, bis dieser terminiert. Anschließend wird der nächste auswählbare Pro- zess analog zur Even-Strategie ausgewählt. In diesem Versuch terminieren Programme noch nicht, jedoch wird dies in späteren Versuchen möglich sein.

### Round Robin
Bei der RoundRobin-Strategie verwaltet der Scheduler eine Zeitscheibe für jeden aktuell laufenden Prozess. Diese Zeitscheibe wird mit der Priorität priority des Prozesses initialisiert und bei jedem weiteren Scheduleraufruf dekrementiert, wodurch der Prozess priority-mal ausgewählt wird; also bis die Zeitscheibe den Wert 0 erreicht. In diesem Fall wählt der Scheduler den nächsten auswählbaren Prozess analog zur Even- Strategie aus.

### Inactive Aging
Zusätzlich zur Priorität erhält jeder Prozess bei dieser Strategie ein Alter, das mit 0 initialisiert wird. Mit jedem Aufruf des Schedulers wächst das Alter jedes Prozesses um dessen Priorität. Es wird immer der älteste Prozess als aktiver Prozess ausgewählt, wobei sein Alter zurückgesetzt wird. Die anderen, inaktiven Prozesse altern jedoch weiter, sodass garantiert ist, dass auch ein niedrigpriorisierter Prozess irgendwann wegen seiner langen Wartezeit ausgewählt wird.

Diese Strategie kann schrittweise beschrieben werden:  
1. Das Alter aller Prozesse wird um deren Priorität erhöht
2. Der älteste Prozess wird ausgewählt und dessen Alter auf 0 zurückgesetzt. Exis- tieren mehrere Prozesse mit dem gleichen Alter, wird der höchstpriorisierte aus- gewählt. Existieren mehrere Prozesse mit gleichem Alter und Priorität, wird der Prozess mit der kleinsten Prozess-ID ausgewählt.
