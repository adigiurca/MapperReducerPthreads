Giurca Ionut-Adrian
Gr. 334AB

Calculul paralel al unui index inversat folosind paradigma Map-Reduce

Am ales sa rezolv aceasta problema folosind C++. 
Programul foloseste set-uri pentru crearea structurilor de tip {cuvant, fileID} si un hash-map pentru crearea structurilor
{cuvant, {fileID1, fileID2, ...}}. 

Faza de map incepe cu citirea fisierelor de intrare atribuite fiecarui mapper in parte. Impartirea acestora este relativ egala la 
mapperi intrucat numarul de fisiere este impartit la numarul de mapperi. 
Fiecare mapper:
	- citeste fisierele atribuite lui
	- prelucreaza cuvintele pentru a le transforma in cuvinte fara majuscule si fara caractere non-alfanumerice (That's -> thats)
	- mapeaza fiecare cuvant la fisierID-ul fisierului din care a provenit si le stocheaza in set-ul mentionat mai sus care este
	  protejat de un mutex pentru a evita accesul concurent
	- paralelizarea in aceasta faza consta in procesarea unui subset diferit de fisiere simultan

Faza de reduce incepe dupa ce este asigurata finalizarea fazei de map printr-o bariera intrucat reducerii proceseaza datele complete si
corecte.
Fiecare reducer:
	- proceseaza cuvintele prelucrate care incep cu literele asignate reducer-ului respectiv (ex. un reducer proceseaza cuvintele
	  care incep cu a, b, c, d iar alt reducer de la e la h etc.)
	- foloseste un mutex pentru accesul sigur la datele colectate de mapperi
	- sorteaza cuvintele descrescator in functie de numarul de fisiere din care provin si crescator in cazul in care numarul de 	 	  fisiere este egal
	- paralelizarea in aceasta faza consta in prelucarea simultana a mai multor intervale diferite de litere de catre reduceri


Aceasta implementare are un defect intrucat asignarea fisierelor de procesare la mapperi poate fi realizata mai eficient.

Rularea programului se face in urmatorul fel:
	./tema1 M R file_in.txt (unde M reprezinta numarul de thread-uri mapper iar R numarul de thread-uri reducer)
	