
Vorrei creare un progetto rust (all'interno della cartella rust) che facesse questo:

Il progetto dovrà generare 2 binari:
Il binario principale deve presentare il seguente menu tramite cli:

 * Nuovo progetto
 * Gestione progetto
   * In questo caso l'utente deve indicare la cartella del progetto da gestire. (vedi le informazioni sotto per la generazione del progetto, così da chiedere all'utente di verificarle e modificarle. Inoltre deve essere possibile poter fare il clean del database - vedi sotto - per resettare il cron job al passaggio successivo).
 * Quando si inizia un nuovo progetto l'utentde deve inserire le credenziali di google cloud api:
   * il path del file .json (che viene scaricato da google cloud api console). Vedi per riferimento questo file json: /Users/alessandro/Downloads/arduino-458815-9e4f1267fc95.json
   * A questo punto il programma deve convalidare le credenziali google per verificare se sono corrette e se l'utente ha i permessi necessari per:
     * gestire album di google photo
     * leggere e scrivere su google drive
   * Chiedere il nome dell'album che si vuole creare su google photo (e verificare se l'album esiste già, in tal caso bisogna usare un altro nome)
   * Chiedere la lista di email con cui condividere l'album di google photo
   * Chiedere il percorso della cartella su google drive da utilizzare. Se non esiste bisogna crearla e assicurarsi che sia condivisa con il service_account.
   * Chiedere la lista di email con cui condividere la cartella google drive.
   * Chiedere all'utente il percorso del binario "processor" di rust che verrà poi utilizzato dal secondo binario.
   * Chiedere all'utente le informazioni per il processing delle immagini:
   * la linea di comando completa per l'esecuzione del processor, esclusi i parametri "--input" e "--output" che verranno poi aggiunti dal secondo binario durante il suo flow.
   * Chiedere ogni quanto il cron-job deve essere eseguito (1 volta al giorno, una volta la settimana, una al mese)
   * Chiedere se si vuole salvare il progetto o rivedere le informazioni per modificarle.
 * A questo punto verrà generato un file .properties temporaneo che verrà utilizzato dal secondo binario per testare che tutte le informazioni siano corrette. Verrà chiesto all'utente se si vuole prima testare la configurazione attuale. In tal caso si lancerà direttamente il secondo binario, ma facendo in modo che venga limitato alla lettura di 4 immagini al massimo. In questo modo verrà testato il flow corretto del secondo binario (che in questa fase di test non deve generare il database o cercare di leggere dal database perchè in modalità test) che deve anche fare l'upload dei file su google drive. Una volta terminato il test di indicherà il percorso e i nomi dei file generati su google drive, così che l'utente possa verificarne il contenuto corretto. Se l'utenti conferma che tutto è corretto i file devono essere rimossi.
 * Se tutto va a buon fine si salverà il progetto utilizzando un file .properties con tutte le informazioni necessarie (nella cartella del progetto indicata all'inizio) e verrà generato un cron-job sul sistema dell'utente che punterà al secondo binario che verrà generato dal progetto rust. Il quale accetterà come parametro di input il file .properties salvato.
 * Il secondo binario utilizzerà un database per tenere conto delle immagini della cartella google photo già processate, così da non processarle una seconda volta.
 * Quando il secondo binario verrà lanciato dal cron del sistema dovrà leggere la lista delle immagini non ancora processate, scaricare quelle nuove in una cartella temporanea, eseguire il processing tramite il processor rust utilizzando la cartella temporanea come input e una seconda cartella temporanea come output. Passare al processor rust l'argomento `--report json` e leggere l'output di report per verificare quali immagini sono state processate e quali no. Le immagini processate verranno quindi aggiunte nel database.
 * Fare l'upload dei file nella cartella di output nella cartella di google drive.

Note tecniche:
Utilizzare clap, indicatif, ratatui, dialoguer
Utilizzare rust/photoframe_lib e rust/processor per verificare i tipi per gli argomenti da usare quando di dovrà poi invocare il processor.

Prima di iniziare a create il progetto dimmi se è tutto fattibile e indicami quali step utilizzerai per la generazione dei task.
Dobbiamo fare in modo di usare piccoli step alla volta, con un massimo di 200 righe di codice per volta così da poter testare il progresso.