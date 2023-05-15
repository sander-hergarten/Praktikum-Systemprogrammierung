Ein Scheduler wird in Multitasking Betriebssystemen eingesetzt, um das Betriebsmittle Prozessorzeit zu verwalten und den Wechsel zwischen [[Prozess|Prozessen]] transparent durchzufuehren. 

### Transparenz
Transparenz bedeutet in diesem Fall, dass jedem Prozess Prozessorzeit und Stack- speicher idealerweise so zur Verfügung stehen, dass der Prozess nicht unterscheiden kann, ob er exklusiven Zugriff auf das System hat oder nicht.

Dies kann ereicht werden durch z.B. Periodisches unterbrechen des Programmes wonach ein anderes reingewechselt wird.

Um zwischen Prozessen wechseln zu koennen muss der [[Laufzeitkontext]] des aktiven Prozesses gespeichert werden und der des naechsten prozesses wiederhergestellt werden.

In dem Fall wo kein Prozess ausgewaehlt werden kann wird ein Leerlauf Prozess ausgewaehlt.