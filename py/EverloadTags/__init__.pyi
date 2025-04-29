from PySide6.QtCore import QMargins, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QWidget, QAbstractScrollArea
import typing

class BehaviorConfig:
    unique: bool

class StyleConfig:
    # Padding from the text to the the pill border
    pill_thickness: QMargins = QMargins(7, 7, 8, 7)

    # Space between pills
    pills_h_spacing: int = 7

    # Size of cross side
    tag_cross_size: int = 8

    # Distance between text and the cross
    tag_cross_spacing: int = 3

    color = QColor(255, 164, 100, 100)

    # Rounding of the pill
    rounding_x_radius: int = 5

    # Rounding of the pill
    rounding_y_radius: int = 5

class Config:
    style: StyleConfig
    behavior: BehaviorConfig

class TagsLineEdit(QWidget):
    tagsEdited: typing.ClassVar[Signal] = ...
    def __init__(
        self, parent: QWidget | None = ..., config: Config | None = ...
    ) -> None: ...
    def completion(self, completions: list[str]) -> None: ...  # Set completions
    @typing.overload
    def tags(self, tags: list[str]) -> None: ...  # Set tags
    @typing.overload
    def tags(self) -> list[str]: ...  # Get tags
    @typing.overload
    def config(self, config: Config) -> None: ...  # Set config
    @typing.overload
    def config(self) -> Config: ...  # Get config

class TagsEdit(QAbstractScrollArea):
    tagsEdited: typing.ClassVar[Signal] = ...
    def __init__(
        self, parent: QWidget | None = ..., config: Config | None = ...
    ) -> None: ...
    def completion(self, completions: list[str]) -> None: ...  # Set completions
    @typing.overload
    def tags(self, tags: list[str]) -> None: ...  # Set tags
    @typing.overload
    def tags(self) -> list[str]: ...  # Get tags
    @typing.overload
    def config(self, config: Config) -> None: ...  # Set config
    @typing.overload
    def config(self) -> Config: ...  # Get config
