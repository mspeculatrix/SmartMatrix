# SmartMatrix Webserver

This very simple website is intended to serve as a front end for the SmartMatrix parallel printer device.

You can run this on your laptop/desktop to allow you to print files on that machine. Or you can run it on a home server, accessible by multiple machines over the network. In the latter case, however, the `public_html/files/` directory needs to be accessible and writable over the network, too. You could try setting up an SMB or NFS share and converting the `public_html/files/` directory into a symlink to it.

It's pretty straightforward. The webpage includes JavaScript that responds to button clicks by sending GET requests to `apiserver.php`. This takes appropriate actions - in most cases by using a raw socket connection to the SmartMatrix to send bytes.

The `public_html/files/` folder is where you can put plain text files in order to print them. This needs to stay inside the `public_html/` folder. However, you can put the top-level `webserver/` folder wherever you like.

The site is built with Bootstrap and FontAwesome (free versions). All the files are included. I chose not to use the CDN approach because I don't like local apps like this being dependent on an internet connection.

In part, this site was created by reusing code from other projects, so it's possible there are some redundant parts. For example, I'm not sure we're using all the fonts in the `assets/fonts/` folder. Feel free to experiment by removing things until it breaks.

## Configuration

You need to set the IP address and port number of the SmartMatrix device in `printsvr.cfg`. Note, these are the details of the SmartMatrix, not this webserver.

By default, the SmartMatrix port is `9100`, unless you have changed it in the firmware (which I suggest you don't). The IP address will be the one dished out bby whatever DHCP server you have running on your local network. It will be shown on the SmartMatrix's OLED panel.

## Docker

The site runs using Docker. You can put the webserver folder anywhere on your system - eg, in the place you like to keep Docker projects.

The container is not entirely self-contained. The Apache `/var/www/html/` inside the container is mapped to the `public_html/` folder. That makes it considerably easier to work on the site. It also makes the `public_html/files/` folder more accessible.

Assuming your user is part of the `docker` group and you have all the necessary Docker stuff installed, then in a terminal `cd` to the webserver directory (wherever you put it) and run:

```
docker-compose up -d --build
```

Whenever you restart the container in the future (and assuming you haven't made any changes that would require a rebuild) you can just run:

```
docker-compose up -d
```

You can then visit the site by pointing your browser to:

```
// if you're running it on the same machine that you're running the browser,
// such as your laptop:
http://localhost:8989

// if you're running it on another machine on your local network at
// IP address xxx.xxx.xxx.xxx:
http://xxx.xxx.xxx.xxx:8989

```

Note the added port number, `8989`. You'll see this is assigned in the `docker-compose.yaml` file. But this can be any port you like, so long as it's not in use by something else. If the machine you're running it on doesn't have a website running on port 80, I'd suggest using that because it simplifies the URL - eg: `http://localhost`.

## SCSS

The `scss` folder contains the files needed to modify the CSS for the site using SASS.

If you don't want to mess with the CSS, you can ignore this folder.

If you're happy to directly edit the `public_html/css/main.css` file, you can also ignore this folder (although you will want to delete the `public_html/css/main.css.map` file).

If you're a SASS fan, you know what to do.
