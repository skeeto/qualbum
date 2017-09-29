#!/usr/bin/env python3
import mistune
import os
import uuid
import yaml
from bs4 import BeautifulSoup
from datetime import datetime, timezone
from multiprocessing import Pool
from PIL import Image

def mkdir_p(path):
    """Like mkdir -p"""
    try:
        os.makedirs(path)
    except:
        pass

def link(src, dst):
    """Like ln -f."""
    if os.path.exists(dst):
        os.unlink(dst)
    os.link(src, dst)

def newer(a, b):
    """Return true if b exists and is older than a."""
    return (not os.path.exists(b) or
            os.path.getmtime(a) > os.path.getmtime(b))

def route_to_path(route, config):
    parts = route.split('/')
    return os.path.join(config.destination, *parts)

class Config:
    def __init__(self, yamlfile=None):
        if yamlfile:
            with open(yamlfile, 'r', encoding='utf-8') as file:
                config = yaml.load(file)
        else:
            config = {}
        self.title = config.get('title', 'Qualbum')
        self.author = config.get('author', 'Nobody')
        self.baseurl = config.get('baseurl', 'http://example.com')
        self.destination = config.get('destination', '_site')
        thumbsize = config.get('thumbsize', 300)
        previewsize = config.get('thumbsize', 1200)
        self.thumbsize = thumbsize, thumbsize
        self.previewsize = previewsize, previewsize

class Photo:
    def __init__(self, metafile, config=Config()):
        self.config = config
        with open(metafile, 'r', encoding='utf-8') as f:
            header = [f.readline()]
            while True:
                line = f.readline()
                if line.startswith('---'):
                    break;
                header.append(line)
            markdown = ''.join(f.readlines())
        meta = yaml.load(''.join(header))
        self.title = meta.get('title', '?')
        self.date = meta.get('date', datetime.now(timezone.utc).astimezone())
        self.meta = meta
        html = mistune.markdown(markdown)
        self.dom = BeautifulSoup(html, 'html.parser')
        base = os.path.normpath(os.path.splitext(metafile)[0])
        self.photo_file = base + '.jpg'
        parts = base.split(os.sep)
        self.route = '/' + '/'.join(parts) + '/'
        parts = os.path.normpath(self.photo_file).split(os.sep)
        self.photo_route = '/' + '/'.join(parts)
        self.href = None

    def create_page(self, prev=None, next=None):
        dom = BeautifulSoup(open('_single.html', encoding='utf-8'), 'xml')

        dom.select('title')[0].string = self.title
        dom.select('#title')[0].string = self.title

        a = dom.select('#full')[0]
        a.attrs['href'] = self.photo_route

        img = dom.select('#photo')[0]
        img.attrs['src'] = 'preview.jpg'

        if prev:
            dom.select('#prev')[0].attrs['href'] = prev.route
        if next:
            dom.select('#next')[0].attrs['href'] = next.route

        time = dom.select('time')[0]
        time.string = self.date.strftime('%B %d, %Y')
        time.attrs['datetime'] = self.date.isoformat() + 'Z'

        if self.meta.get('f-stop'):
            dom.select('#f-stop')[0].string = self.meta['f-stop']
        if self.meta.get('exposure-time'):
            exposure_time = dom.select('#exposure-time')[0]
            exposure_time.string = self.meta['exposure-time']
        if self.meta.get('iso'):
            dom.select('#iso')[0].string = str(self.meta['iso'])
        dom.select('#info')[0].append(self.dom)

        base = route_to_path(self.route, self.config)
        mkdir_p(base)
        index = os.path.join(base, 'index.html')
        with open(index, 'w', encoding='utf-8') as f:
            f.write(dom.prettify())

    def create_resized(self):
        thumbnail_file = route_to_path(self.route + 'thumb.jpg', self.config)
        preview_file = route_to_path(self.route + 'preview.jpg', self.config)
        image = Image.open(self.photo_file)
        width, height = image.size
        if width > height:
            pad = width - height
            x0 = pad // 2
            y0 = 0
            x1 = height + x0
            y1 = height
        else:
            pad = height - width
            x0 = 0
            y0 = pad // 2
            x1 = width
            y1 = width + y0
        cropped = image.crop((x0, y0, x1, y1))
        cropped.thumbnail(self.config.thumbsize, Image.ANTIALIAS)
        mkdir_p(os.path.dirname(thumbnail_file))
        cropped.save(thumbnail_file)
        image.thumbnail(self.config.previewsize)
        image.save(preview_file)

class Feed:
    def __init__(self, route, title=None, config=Config()):
        self.route = route + 'feed/'
        self.title = title
        self.config = config
        self.id = uuid.uuid3(uuid.NAMESPACE_URL, config.baseurl + route)
        self.photos = []

    def append(self, photo):
        self.photos.append(photo)

    def close(self):
        baseurl = self.config.baseurl
        dom = BeautifulSoup(open('_feed.xml', encoding='utf-8'), 'xml')
        feed = dom.select('feed')[0]
        title = dom.select('title')[0]
        if self.title:
            title.string = self.title + ' » ' + self.config.title
        else:
            title.string = self.config.title
        dom.select('author name')[0].string = self.config.author
        updated = dom.select('feed > updated')[0]
        updated.string = datetime.now(timezone.utc).astimezone().isoformat()

        dom.select('feed > id')[0].string = 'urn:uuid:' + str(self.id)

        linkself = dom.select('feed link[rel="self"]')[0]
        linkself.attrs['href'] = self.config.baseurl + self.route

        for photo in self.photos:
            entry = dom.new_tag('entry')
            title = dom.new_tag('title')
            title.string = photo.title

            id = dom.new_tag('id')
            url = self.config.baseurl + (photo.href or photo.route)
            id.string = 'urn:uuid:' + str(uuid.uuid3(uuid.NAMESPACE_URL, url))

            link = dom.new_tag('link')
            link.attrs['rel'] = 'alternate'
            link.attrs['type'] = 'text/html'
            link.attrs['href'] = url

            updated = dom.new_tag('updated')
            updated.string = photo.date.isoformat() + 'Z'

            content = dom.new_tag('content')
            content.attrs['type'] = 'html'
            content.string = str(photo.dom)

            entry.append(title)
            entry.append(id)
            entry.append(link)
            entry.append(updated)
            entry.append(content)
            feed.append(entry)

        dest = route_to_path(self.route, self.config)
        mkdir_p(dest)
        xml = os.path.join(dest, 'index.xml')
        with open(xml, 'w', encoding='utf-8') as file:
            file.write(str(dom))

class Gallery:
    def __init__(self, yamlfile=None, config=Config()):
        self.config = config
        self.photos = []
        if yamlfile:
            with open(yamlfile, 'r', encoding='utf-8') as file:
                meta = yaml.load(file)
            self.path = os.path.normpath(os.path.dirname(yamlfile))
            parts = os.path.normpath(self.path).split(os.sep)
            self.route = '/' + '/'.join(parts) + '/'
        else:
            meta = {}
            self.route = '/'
        self.title = meta.get('title', config.title)
        self.image = meta.get('image')
        with open('_gallery.html', 'r', encoding='utf-8') as template:
            self.dom = BeautifulSoup(template, 'xml')
        self.gallery = self.dom.select('#gallery')[0]

    def close(self):
        self.photos = sorted(self.photos, key=lambda p: p.date, reverse=True)
        feed = Feed(self.route, self.title, config=self.config)
        for photo in self.photos:
            feed.append(photo)
            li = self.dom.new_tag('li')
            h2 = self.dom.new_tag('h2')
            h2.string = photo.title
            li.append(h2)
            img = self.dom.new_tag('img')
            img.attrs['src'] = photo.route + 'thumb.jpg'
            img.attrs['alt'] = ''
            img.attrs['title'] = photo.title
            img.attrs['width'] = str(self.config.thumbsize[0])
            img.attrs['height'] = str(self.config.thumbsize[1])
            a = self.dom.new_tag('a')
            a.attrs['href'] = photo.href or photo.route
            a.append(img)
            li.append(a)
            self.gallery.append(li)
        feed.close()
        dom_title = self.dom.select('title')[0]
        if self.title == self.config.title:
            dom_title.string = self.title
        else:
            dom_title.string = self.title + ' » ' + self.config.title
        self.dom.select('#title')[0].string = self.title
        dest = route_to_path(self.route, self.config)
        mkdir_p(dest)
        index = os.path.join(dest, 'index.html')
        with open(index, 'w', encoding='utf-8') as file:
            file.write(self.dom.prettify())
        self.dom = None

    def append(self, photo):
        self.photos.append(photo)

    def gather(self):
        for file in os.listdir(self.path):
            if file.endswith('.md'):
                meta = os.path.join(self.path, file)
                photo = Photo(meta, config=self.config)
                self.append(photo)

class ResizeManager:
    def __init__(self):
        self.photos = []

    def append(self, photo):
        route = photo.route
        config = photo.config
        thumbnail_file = route_to_path(route + 'thumb.jpg', config)
        preview_file = route_to_path(route + 'preview.jpg', config)
        newer_thumbnail = newer(photo.photo_file, thumbnail_file)
        newer_preview = newer(photo.photo_file, preview_file)
        if newer_thumbnail or newer_preview:
            self.photos.append(photo)

    def process(self):
        with Pool() as pool:
            pool.map(Photo.create_resized, self.photos, 1)

def generate():
    """Generate a site from the current directory."""
    config = Config('_config.yaml')
    main = Gallery(config=config)
    main.title = config.title
    galleries = []
    dotfile = os.path.join('.', '.')
    underfile = os.path.join('.', '_')
    for root, dirs, files in os.walk('.'):
        if not root.startswith(dotfile) and not root.startswith(underfile):
            for file in files:
                path = os.path.join(root, file)
                if file == '_gallery.yaml':
                    gallery = Gallery(path, config)
                    gallery.gather()
                    for photo in gallery.photos:
                        main.append(photo)
                    gallery.close()
                    galleries.append(gallery)
                elif file[0] != '_':
                    dest = os.path.join(config.destination, path)
                    mkdir_p(os.path.dirname(dest))
                    link(path, dest)
    main.close()

    # Build individual image pages
    resizer = ResizeManager()
    for i, photo in enumerate(main.photos):
        prev = i > 0 and main.photos[i - 1]
        next = i < len(main.photos) - 1 and main.photos[i + 1]
        photo.create_page(prev=prev, next=next)
        resizer.append(photo)
    resizer.process()

    # Build albums "gallery"
    albums = Gallery(config=config)
    albums.title = 'List of Albums'
    albums.route = '/albums/'
    for gallery in galleries:
        photo = gallery.photos[-1];
        if gallery.image:
            for candidate in gallery.photos:
                if candidate.title == gallery.image:
                    photo = candidate
        photo.title = gallery.title
        photo.href = gallery.route
        albums.append(photo)
    albums.close()

if __name__ == '__main__':
    generate()
