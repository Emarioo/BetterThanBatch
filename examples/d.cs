using (var reader = new StreamReader("products.csv"))
{
    // Läs in antal böcker, dataspel och filmer
    int numBooks = int.Parse(reader.ReadLine());
    int numGames = int.Parse(reader.ReadLine());
    int numMovies = int.Parse(reader.ReadLine());

    // Läs in böcker
    int vnum = 0, stock = 0;
    for (int i = 0; i < numBooks; i++)
    {
        var line = reader.ReadLine();
        var values = line.Split(',');
        vnum = int.Parse(values[0]);
        stock = int.Parse(values[1]);

        Product book = new Bok(values[2], int.Parse(values[3]), values[4], values[5], values[6], values[7]);
        book.Stock = stock;
        book.ItemNumber = vnum;
        products.Add(book);
    }

    // Läs in dataspel
    for (int i = 0; i < numGames; i++)
    {
        var line = reader.ReadLine();
        var values = line.Split(',');
        vnum = int.Parse(values[0]);
        stock = int.Parse(values[1]);

        Product spel = new Dataspel(values[2], int.Parse(values[3]), values[4]);
        spel.Stock = stock;
        spel.ItemNumber = vnum;
        products.Add(spel);
    }

    // Läs in filmer
    for (int i = 0; i < numMovies; i++)
    {
        var line = reader.ReadLine();
        var values = line.Split(',');
        vnum = int.Parse(values[0]);
        stock = int.Parse(values[1]);

        Product film = new Movie(values[2], int.Parse(values[3]), values[4], int.Parse(values[5]));
        film.Stock = stock;
        film.ItemNumber = vnum;
        products.Add(film);
    }
}