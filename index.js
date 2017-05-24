
// Aqui solo seguimos la documentacion de goolge charts
google.charts.load('current', {'packages':['corechart']});
google.charts.setOnLoadCallback(drawChart);

function drawChart() {

  // tenemos que hacer uso de ajax para poder leer de un fichero de nuestro sistema
  // Especificamos la ruta y el tipo de datos que contiene, en nuestro caso, texto.
  $.ajax({

    url: './sensor_data.txt',
    dataType: 'text',
    success: function(txt) {

      // en caso de que lo hayamos leido alojamos el contenido en txt

      // estas variables seran las que usara charts para pintar las graficas, a partir de aqui iremos
      // añadiendo la hora y los datos.
      var dataTemperatura = [['Hora', 'Temperatura(ºC)']];
      var dataPresion = [['Hora', 'Presion(Pascales)']];

      // El fichero guarda los datos(ver README) separados, primero, por intervalos de tiempo en linea,
      // segundo, cada columna, con separacion de 1 espacio,  representa hora, temperatura y presion
      var txtArray = txt.split('\n'); // primero guardamos arrays por lineas

      // a continuacion pasamos a inspeccionar linea a linea los datos, los vamos guardando en el array que le corresponda
      for( var i = 0; i < txtArray.length; i++) {

        var auxArray = txtArray[i].split(" "); // como tenemos dos datos separados por espacio, los separamos en otro array

        dataTemperatura.push( [ auxArray[0], parseInt(auxArray[1]) ]); // primero temperatura
        dataPresion.push( [ auxArray[0], parseInt(auxArray[2]) ]); // luego presion

      }

      // Una vez tenemos los datos, seguimos la documentacion de google y ya lo tenemos:

      // Grafico de temperatura
      var data_temperatura = google.visualization.arrayToDataTable(dataTemperatura);
      var options_temp = {
        title : 'Temperatura',
        curveType : 'function',
        colors : ['#F44336'],
        vAxis : { format : '# ºC'},
        legend : { position: 'bottom' }
      };

      var chart_temp = new google.visualization.LineChart(document.getElementById('grafico_temperatura'));

      chart_temp.draw(data_temperatura, options_temp);

      // Grafico de presion
      var data_pres = google.visualization.arrayToDataTable(dataPresion);
      var options_pres = {
        title: 'Presion',
        curveType: 'function',
        colors: ['#3F51B5'],
        vAxis : { format : '# P'},
        legend: { position: 'bottom' }
      };

      var chart_pres = new google.visualization.LineChart(document.getElementById('grafico_presion'));

      chart_pres.draw(data_pres, options_pres);

    }

  });
}