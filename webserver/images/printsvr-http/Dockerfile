FROM php:8.2-apache

# Install required PHP extensions and enable Apache modules
RUN docker-php-ext-install sockets && a2enmod headers

# Copy virtual host config
COPY 000-default.conf /etc/apache2/sites-available/

# Expose HTTP port
EXPOSE 80
