#!/usr/bin/env python3

from asciimatics.widgets import Frame, ListBox, Layout, Divider, Text, \
    Button, TextBox, Widget, PopUpDialog
from asciimatics.scene import Scene
from asciimatics.screen import Screen
from asciimatics.exceptions import ResizeScreenError, NextScene, StopApplication
from asciimatics.event import KeyboardEvent
import sys
import os
from ctypes import *
import mysql.connector
from mysql.connector import Error


class Card(Structure):
    pass
CardPtr = POINTER(Card)

class Contact(Structure):
    _fields_ = [
        ("file_name", c_char * 60),
        ("name", c_char * 256),
        ("birthday", c_char * 256),
        ("anniversary", c_char * 256),
        ("prop_count", c_int)
    ]

VCAPI = CDLL("libvcparser.so")

createCard = VCAPI.createCard
createCard.argtypes = [c_char_p, POINTER(CardPtr)]
createCard.restype = c_int

validateCard = VCAPI.validateCard
validateCard.argtypes = [CardPtr]
validateCard.restype = c_int

errorToString = VCAPI.errorToString
errorToString.argtypes = [c_int]
errorToString.restype = c_char_p

getContact = VCAPI.getContact
getContact.argtypes = [c_char_p, CardPtr]
getContact.restype = Contact

updateName = VCAPI.updateName
updateName.argtypes = [c_char_p, c_char_p, POINTER(CardPtr)]
updateName.restype = c_int

newCard = VCAPI.newCard
newCard.argtypes = [c_char_p, c_char_p, POINTER(CardPtr)]
newCard.restype = c_int 

encodeDate = VCAPI.encodeDate
encodeDate.argtypes = [c_char_p]
encodeDate.restype = c_char_p

decodeDate = VCAPI.decodeDate
decodeDate.argtypes = [c_char_p]
decodeDate.restype = c_char_p

class ContactModel:
    def __init__(self, db_connection):
        self.contacts = []
        self.cardPtrs = []
        self.current_id = None
        self.db = db_connection
        self.load_contacts()

    def load_contacts(self):
        card_dir = "cards/"
        if not os.path.exists(card_dir):
            os.makedirs(card_dir)
            
        vcf_files = [f for f in os.listdir(card_dir) if f.endswith(".vcf")]

        for file in vcf_files:
            full_path = self.get_full_path(file)
            card_ptr = CardPtr()

            err = createCard(full_path, byref(card_ptr))
            if err != 0:
                err_str = errorToString(err)
                continue

            err = validateCard(card_ptr)

            if err != 0:
                err_str = errorToString(err)
                continue
                
            contact = getContact(file.encode('utf-8'), card_ptr)
            contact.birthday = decodeDate(contact.birthday) if contact.birthday else b""
            contact.anniversary = decodeDate(contact.anniversary) if contact.anniversary else b""
            
            self.contacts.append(contact)
            self.cardPtrs.append(card_ptr)
            
            if self.db:
                self.insert_contact_db(file, contact)

    def update_current_contact(self, contact_data):
        filename = contact_data["file_name"].strip()
        name = contact_data["name"].strip()
        
        if not filename or not name:
            return
        if not filename.endswith(".vcf"):
            return
        
        if self.current_id is None:
            self.insert_contact(filename, name)
            self.current_id = len(self.contacts) - 1
        else:
            self.update_name(name)

    def insert_contact(self, filename, name, birthday="", anniversary=""):
        if not filename.endswith(".vcf"):
            return
            
        full_path = self.get_full_path(filename)
            
        encoded_bday = encodeDate(birthday.encode('utf-8')) if birthday else None
        encoded_anniv = encodeDate(anniversary.encode('utf-8')) if anniversary else None
            
        card_ptr = CardPtr()
        if newCard(full_path, name.encode('utf-8'), byref(card_ptr)) != 0:
            return
            
        contact = getContact(filename.encode('utf-8'), card_ptr)
        contact.birthday = decodeDate(encoded_bday) if encoded_bday else b""
        contact.anniversary = decodeDate(encoded_anniv) if encoded_anniv else b""
        
        self.contacts.append(contact)
        self.cardPtrs.append(card_ptr)
        
        if self.db:
            self.insert_contact_db(filename, contact)

    def update_name(self, new_name):
        if self.current_id is None or self.current_id >= len(self.contacts):
            return
            
        contact = self.contacts[self.current_id]
        old_name = contact.name.decode("utf-8")
        filename = contact.file_name.decode("utf-8")
        
        full_path = self.get_full_path(filename)
        if updateName(full_path, new_name.encode('utf-8'), byref(self.cardPtrs[self.current_id])) != 0:
            return
        
        contact.name = new_name.encode('utf-8')
        
        if self.db:
            self.update_name_db(filename, old_name, new_name)
        
    def insert_contact_db(self, filename, contact):
        cursor = None
        try:
            cursor = self.db.cursor()

            cursor.execute(
                "SELECT file_id FROM FILE WHERE file_name = %s",
                (filename,)
            )
            results = cursor.fetchall()
            if not results:
                cursor.execute("""
                    INSERT INTO FILE (file_name, last_modified, creation_time)
                    VALUES (%s, NOW(), NOW())
                """, (filename,))
                file_id = cursor.lastrowid
                
                cursor.execute("""
                    INSERT INTO CONTACT (name, birthday, anniversary, file_id)
                    VALUES (%s, NULLIF(%s, ''), NULLIF(%s, ''), %s)
                """, (
                    contact.name.decode('utf-8'),
                    contact.birthday.decode('utf-8') if contact.birthday else '',
                    contact.anniversary.decode('utf-8') if contact.anniversary else '',
                    file_id
                ))
                
                self.db.commit()
        except Exception as e:
            self.db.rollback()
            raise e
        finally:
            if cursor:
                cursor.close()

    def update_name_db(self, filename, old_name, new_name):
        cursor = None
        try:
            cursor = self.db.cursor()
            
            cursor.execute("SELECT file_id FROM FILE WHERE file_name = %s", (filename,))
            file_id = cursor.fetchone()[0]
            
            cursor.execute("""
                UPDATE CONTACT SET name = %s
                WHERE file_id = %s AND name = %s
            """, (new_name, file_id, old_name))
            
            cursor.execute("""
                UPDATE FILE SET last_modified = NOW()
                WHERE file_id = %s
            """, (file_id,))
            
            self.db.commit()
        except Exception as e:
            self.db.rollback()
            raise e
        finally:
            if cursor:
                cursor.close()

    def get_summary(self):
        summary = []
        for id, contact in enumerate(self.contacts):
            display_str = contact.file_name.decode('utf-8')
            summary.append((display_str, id))
        return summary

    def get_current_contact(self):
        if self.current_id is None or self.current_id >= len(self.contacts):
            return {
                "file_name": "",
                "name": "",
                "birthday": "",
                "anniversary": "",
                "prop_count": ""
            }
        contact = self.contacts[self.current_id]
        return {
            "file_name": contact.file_name.decode("utf-8"),
            "name": contact.name.decode("utf-8"),
            "birthday": contact.birthday.decode("utf-8"),
            "anniversary": contact.anniversary.decode("utf-8"),
            "prop_count": str(contact.prop_count)
        }

    def get_full_path(self, file_name):
        card_dir = "cards/"
        full_path = os.path.join(card_dir, file_name).encode('utf-8')
        return full_path


class LoginView(Frame):
    def __init__(self, screen):
        super(LoginView, self).__init__(screen,
                                      screen.height * 2 // 3,
                                      screen.width * 2 // 3,
                                      hover_focus=True,
                                      title="Login to Database")
        self._screen = screen
        self.db_connection = None
        
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(Text("Username:", "username"))
        layout.add_widget(Text("Password:", "password")) #, hide_char="*"
        layout.add_widget(Text("DB name:", "db_name"))
        
        layout2 = Layout([1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._login), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        
        self.fix()

    def process_event(self, event):
        if isinstance(event, KeyboardEvent):
            if event.key_code == -1:
                self._cancel()
        return super(LoginView, self).process_event(event)

    def _login(self):
        self.save()
        try:
            self.db_connection = mysql.connector.connect(
                host="dursley.socs.uoguelph.ca",
                user=self.data["username"],
                password=self.data["password"],
                database=self.data["db_name"]
            )
            self._create_tables()
            raise StopApplication("Login successful")
        except mysql.connector.Error as err:
            self._scene.add_effect(
                PopUpDialog(self._screen, f"Connection failed: {err}", ["OK"])
            )

    def _create_tables(self):
        cursor = self.db_connection.cursor()
        
        try:
            cursor = self.db_connection.cursor()
                        
            cursor.execute("DROP TABLE IF EXISTS CONTACT")
            cursor.execute("DROP TABLE IF EXISTS FILE")
            
            cursor.execute("""
                CREATE TABLE FILE (
                    file_id INT AUTO_INCREMENT PRIMARY KEY,
                    file_name VARCHAR(60) NOT NULL,
                    last_modified DATETIME,
                    creation_time DATETIME NOT NULL
                )
            """)
            
            cursor.execute("""
                CREATE TABLE CONTACT (
                    contact_id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(256) NOT NULL,
                    birthday DATETIME,
                    anniversary DATETIME,
                    file_id INT NOT NULL,
                    FOREIGN KEY (file_id) REFERENCES FILE(file_id) ON DELETE CASCADE
                )
            """)
            
            self.db_connection.commit()
            
        except Exception as e:
            self.db_connection.rollback()

        self.db_connection.commit()
        cursor.close()

    @staticmethod
    def _cancel():
        raise StopApplication("User cancelled login")

class ListView(Frame):
    def __init__(self, screen, model):
        super(ListView, self).__init__(screen,
                                     screen.height * 2 // 3,
                                     screen.width * 2 // 3,
                                     on_load=self._reload_list,
                                     hover_focus=True,
                                     can_scroll=True,
                                     title="vCard List")
        self._model = model
        self._list_view = ListBox(
            Widget.FILL_FRAME,
            model.get_summary(),
            name="contacts",
            add_scroll_bar=True,
            on_change=self._on_pick,
            on_select=self._edit)
        
        self._edit_button = Button("Edit", self._edit)
        self._db_button = Button("DB queries", self._db_queries)
        
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(self._list_view)
        layout.add_widget(Divider())
        
        layout2 = Layout([1, 1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Create", self._add), 0)
        layout2.add_widget(self._edit_button, 1)
        layout2.add_widget(self._db_button, 2)
        layout2.add_widget(Button("Exit", self._quit), 3)
        
        self.fix()
        self._on_pick()

    def process_event(self, event):
        if isinstance(event, KeyboardEvent):
            if event.key_code == -1:
                self._quit()
        return super(ListView, self).process_event(event)

    def _on_pick(self):
        self._edit_button.disabled = self._list_view.value is None

    def _reload_list(self, new_value=None):
        self._list_view.options = self._model.get_summary()
        self._list_view.value = new_value

    def _add(self):
        self._model.current_id = None
        raise NextScene("Edit Contact")

    def _edit(self):
        self.save()
        self._model.current_id = self.data["contacts"]
        raise NextScene("Edit Contact")
    
    def _db_queries(self):
        self.save()
        self._model.current_id = self.data["contacts"]
        raise NextScene("DB Queries")

    @staticmethod
    def _quit():
        raise StopApplication("User pressed quit")

class ContactView(Frame):
    def __init__(self, screen, model):
        super(ContactView, self).__init__(screen,
                                        screen.height * 2 // 3,
                                        screen.width * 2 // 3,
                                        hover_focus=True,
                                        can_scroll=True,
                                        title="vCard Details",
                                        reduce_cpu=True)
        self._model = model
        self._screen = screen
        
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(Text("File name:", "file_name"))
        layout.add_widget(Text("Contact:", "name"))
        layout.add_widget(Text("Birthday:", "birthday", readonly=True))
        layout.add_widget(Text("Anniversary:", "anniversary", readonly=True))
        layout.add_widget(Text("Other Properties:", "prop_count", readonly=True))
        
        layout2 = Layout([1, 1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 3)
        
        self.fix()

    def process_event(self, event):
        if isinstance(event, KeyboardEvent):
            if event.key_code == -1:
                self._cancel()
        return super(ContactView, self).process_event(event)

    def reset(self):
        super(ContactView, self).reset()
        self.data = self._model.get_current_contact()

    def _ok(self):
        self.save()
        self._model.update_current_contact(self.data)
        raise NextScene("Main")

    @staticmethod
    def _cancel():
        raise NextScene("Main")

class DBView(Frame):
    def __init__(self, screen, model):
        super(DBView, self).__init__(screen,
                                   screen.height * 2 // 3,
                                   screen.width * 2 // 3,
                                   hover_focus=True,
                                   can_scroll=True,
                                   title="Database Queries")
        self._model = model
        self._screen = screen
        self._results = TextBox(Widget.FILL_FRAME, 
                              readonly=True, 
                              as_string=True,
                              line_wrap=True)
        
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(self._results)
        
        layout2 = Layout([1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Display all", self._display_all), 0)
        layout2.add_widget(Button("Find June birthdays", self._find_june), 1)
        layout2.add_widget(Button("Cancel", self._cancel), 2)
        
        self.fix()

    def process_event(self, event):
        if isinstance(event, KeyboardEvent):
            if event.key_code == -1:
                self._cancel()
        return super(DBView, self).process_event(event)

    def reset(self):
        super(DBView, self).reset()
        self._results.value = "Run a query to see results here"
        self.save()

    def _display_all(self):
        if not self._model.db:
            return
            
        cursor = self._model.db.cursor()
        cursor.execute("""
            SELECT DISTINCT c.name, c.birthday, c.anniversary, f.file_name
            FROM CONTACT c
            JOIN FILE f ON c.file_id = f.file_id
            ORDER BY c.name
        """)
        
        results = cursor.fetchall()
        headers = ["Name", "Birthday", "Anniversary", "File"]
        self._results.value = "All Contacts:\n" + self._format_as_table(headers, results)

        cursor.close()

    def _find_june(self):
        if not self._model.db:
            return
            
        cursor = self._model.db.cursor()
        cursor.execute("""
            SELECT c.name, c.birthday
            FROM CONTACT c
            JOIN FILE f ON c.file_id = f.file_id
            WHERE MONTH(c.birthday) = 6
            ORDER BY (DATEDIFF(f.last_modified, c.birthday) / 365) DESC
        """)
        
        results = cursor.fetchall()
        headers = ["Name", "Birthday"]
        self._results.value = "June Birthdays:\n" + self._format_as_table(headers, results)

        cursor.close()

    def _format_as_table(self, headers, rows):
        if not rows:
            return "No results found."
        
        str_rows = [[str(item) if item is not None else "N/A" for item in row] for row in rows]
        col_widths = [
            max(len(headers[i]), *(len(row[i]) for row in str_rows))
            for i in range(len(headers))
        ]
        
        separator = "+" + "+".join("_" * (width + 2) for width in col_widths) + "+"
        
        table = [separator]
        
        header_row = "| " + " | ".join(headers[i].ljust(col_widths[i]) for i in range(len(headers))) + " |"
        table.extend([header_row, separator])
        
        for row in str_rows:
            row_str = "| " + " | ".join(row[i].ljust(col_widths[i]) for i in range(len(row))) + " |"
            table.append(row_str)
        
        table.append(separator)
        return "\n".join(table)

    @staticmethod
    def _cancel():
        raise NextScene("Main")

def demo(screen, scene):
    if not hasattr(demo, 'contacts'):
        login_view = LoginView(screen)
        login_scene = Scene([login_view], -1, name="Login")
        
        screen.play([login_scene], stop_on_resize=True, start_scene=login_scene)
        
        if login_view.db_connection is None:
            raise StopApplication("Database connection failed")
        
        demo.contacts = ContactModel(login_view.db_connection)
        demo.contacts.db_connection = login_view.db_connection
    
    scenes = [
        Scene([ListView(screen, demo.contacts)], -1, name="Main"),
        Scene([ContactView(screen, demo.contacts)], -1, name="Edit Contact"),
        Scene([DBView(screen, demo.contacts)], -1, name="DB Queries")
    ]
    
    start_scene = scene if scene else next(s for s in scenes if s.name == "Main")
    
    screen.play(scenes, stop_on_resize=True, start_scene=start_scene, allow_int=True)

last_scene = None
while True:
    try:
        Screen.wrapper(demo, catch_interrupt=True, arguments=[last_scene])
        sys.exit(0)
    except ResizeScreenError as e:
        last_scene = e.scene